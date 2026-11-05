#include "LoomApp/IdPreviews.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/Lane.h"

#include "LoomCore/Build.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPdfDocument>
#include <QScreen>

#include <algorithm>

namespace loom {

namespace {

/// What a preview's build gave, for the UI thread.
struct Built {
    QString root;
    QString tag;
    bool ok = false;      ///< Built (or found) without errors.
    QString pdf;          ///< Project-relative.
    QByteArray bytes;     ///< The PDF, read on the lane so the UI never waits on the disk.
};

/// The page's first page drawn at the screen's size (1 pt = 1/72 in at 96 dpi), sharp on high-DPI
/// screens, and cut to its content. A null image if the PDF can't be read.
QImage render(const QByteArray& bytes)
{
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QPdfDocument document;
    document.load(&buffer);
    if (document.status() != QPdfDocument::Status::Ready || document.pageCount() < 1)
        return {};
    const qreal ratio = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    const QSizeF points = document.pagePointSize(0);
    const QSize pixels = (points * (96.0 / 72.0) * ratio).toSize();
    QImage page = document.render(0, pixels);
    if (page.isNull())
        return {};
    QImage cut = cropToContent(page, static_cast<int>(6 * ratio));
    cut.setDevicePixelRatio(ratio);
    return cut;
}

} // namespace

QImage cropToContent(const QImage& page, int margin)
{
    const QImage image = page.convertToFormat(QImage::Format_ARGB32);
    int left = image.width();
    int right = -1;
    int top = image.height();
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = row[x];
            // White or transparent is paper; anything else is drawn.
            if (qAlpha(pixel) < 16 || (qRed(pixel) > 245 && qGreen(pixel) > 245 && qBlue(pixel) > 245))
                continue;
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    if (right < 0)
        return page;
    const QRect content = QRect(QPoint(left, top), QPoint(right, bottom)).adjusted(-margin, -margin, margin, margin) & image.rect();
    return page.copy(content);
}

IdPreviews::IdPreviews(AppSettings* settings, Lane* buildLane, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_lane(buildLane)
    , m_stop(std::make_shared<std::atomic_bool>(false))
{
}

IdPreviews::~IdPreviews()
{
    m_stop->store(true);
}

void IdPreviews::setProject(const QString& root)
{
    if (root == m_root)
        return;
    m_stop->store(true);
    m_stop = std::make_shared<std::atomic_bool>(false);
    m_root = root;
    m_entries.clear();
    m_wanted.clear();
}

QImage IdPreviews::image(const QString& tag) const
{
    return m_entries.value(tag).image;
}

bool IdPreviews::failed(const QString& tag) const
{
    return m_entries.value(tag).failed;
}

void IdPreviews::request(const QString& tag)
{
    if (m_root.isEmpty() || tag.isEmpty())
        return;
    if (const auto found = m_entries.constFind(tag); found != m_entries.cend() && found->current)
        return;
    m_wanted = tag;
    startNext();
}

void IdPreviews::invalidate()
{
    for (Entry& entry : m_entries)
        entry.current = false;
}

void IdPreviews::startNext()
{
    if (m_running || m_wanted.isEmpty() || !m_lane)
        return;
    m_running = true;
    BuildRequest request;
    request.root = m_root;
    request.target = m_wanted;
    request.preview = true;
    request.latexmk = m_settings ? m_settings->latexmkProgram() : QStringLiteral("latexmk");
    request.timeoutMs = (m_settings ? m_settings->compileTimeoutSeconds() : 120) * 1000;
    request.stop = m_stop;
    request.lockWaitMs = 10'000;
    m_wanted.clear();
    m_lane->run(this, [request]() {
        Built built;
        built.root = request.root;
        built.tag = request.target;
        const BuildResult result = buildTarget(request);
        built.ok = result.outcome == BuildOutcome::Succeeded && !result.pdf.isEmpty();
        built.pdf = result.pdf;
        if (built.ok) {
            QFile file(QDir(request.root).filePath(result.pdf));
            if (file.open(QIODevice::ReadOnly))
                built.bytes = file.readAll();
        }
        return built;
    }, [this](const Built& built) {
        m_running = false;
        if (built.root == m_root) {
            Entry& entry = m_entries[built.tag];
            const bool changed = entry.pdf != built.pdf || entry.failed == built.ok || (built.ok && entry.image.isNull());
            entry.current = true;
            entry.failed = !built.ok;
            if (built.ok && changed) {
                entry.pdf = built.pdf;
                entry.image = render(built.bytes);
            } else if (!built.ok) {
                // An error shows the source again (E2), not an old picture.
                entry.pdf.clear();
                entry.image = QImage();
            }
            if (changed)
                emit ready(built.tag);
        }
        startNext();
    }, -1);
}

} // namespace loom
