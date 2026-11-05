#include "LoomCore/Drafts.h"

#include "LoomCore/ProjectFiles.h"
#include "LoomCore/TomlFields.h"
#include "LoomCore/TomlWriter.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>

#include <algorithm>

namespace loom {

namespace {

const QString draftsFolder = QStringLiteral(".loom/drafts");

/// Whether a draft may be kept for `path`: a project path, and not inside `.loom/` itself.
bool draftable(const QString& path)
{
    return isProjectPath(path) && !path.startsWith(QStringLiteral(".loom/")) && path != QStringLiteral(".loom");
}

/// Reads a draft's `.toml`, beside its text.
/// @param relative The `.toml`'s path under `.loom/drafts/`, e.g. `Statements/4KX7.tex.toml`.
/// @returns The draft without its text, or nothing if it isn't one listDrafts() keeps.
std::optional<Draft> readDraftToml(const QByteArray& bytes, const QString& relative)
{
    try {
        const toml::table table = toml::parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        Draft draft;
        draft.path = QString::fromStdString(table["path"].value_or(std::string()));
        draft.base = QString::fromStdString(table["base"].value_or(std::string()));
        // A draft that doesn't sit where its `path` says is ignored: it can't be told apart from a
        // stray file, and offering it back would put text into the wrong file.
        if (!draftable(draft.path) || draft.path + QStringLiteral(".toml") != relative)
            return std::nullopt;
        if (const toml::value<toml::date_time>* value = table["written"].as_date_time()) {
            const toml::date_time& dt = value->get();
            const QDate day(dt.date.year, dt.date.month, dt.date.day);
            const QTime time(dt.time.hour, dt.time.minute, dt.time.second);
            draft.written = dt.offset ? QDateTime(day, time, QTimeZone::fromSecondsAheadOfUtc(dt.offset->minutes * 60))
                                      : QDateTime(day, time);
        }
        return draft;
    } catch (const toml::parse_error&) {
        return std::nullopt;
    }
}

} // namespace

QString draftPath(const QString& path)
{
    return draftsFolder + u'/' + path;
}

QString writeDraft(const QString& root, const Draft& draft)
{
    if (!draftable(draft.path))
        return QStringLiteral("`%1` can't have a recovery file.").arg(draft.path);
    QString toml = QStringLiteral("path = %1\nbase = %2\n").arg(tomlBasicString(draft.path), tomlBasicString(draft.base));
    if (draft.written.isValid())
        toml += QStringLiteral("written = %1\n").arg(tomlOffsetDateTime(draft.written));
    // The text first: a `.toml` is only ever beside a complete text.
    if (QString error = writeProjectFile(root, draftPath(draft.path), draft.text); !error.isEmpty())
        return error;
    return writeProjectFile(root, draftPath(draft.path) + QStringLiteral(".toml"), toml.toUtf8());
}

QString removeDraft(const QString& root, const QString& path)
{
    if (!draftable(path))
        return {};
    for (const QString& relative : {draftPath(path) + QStringLiteral(".toml"), draftPath(path)}) {
        const std::optional<QString> absolute = resolveProjectPath(root, relative);
        if (absolute && QFileInfo::exists(*absolute) && !QFile::remove(*absolute))
            return QStringLiteral("Loom couldn't remove %1.").arg(QDir::toNativeSeparators(*absolute));
    }
    // Folders the draft leaves empty, up to `.loom/drafts/` itself.
    const std::optional<QString> drafts = resolveProjectPath(root, draftsFolder);
    if (!drafts)
        return {};
    QString folder = QFileInfo(root + u'/' + draftPath(path)).absolutePath();
    const QString top = QDir::cleanPath(*drafts);
    while (QDir::cleanPath(folder).startsWith(top + u'/') && QDir(folder).isEmpty()) {
        if (!QDir().rmdir(folder))
            break;
        folder = QFileInfo(folder).absolutePath();
    }
    return {};
}

std::vector<Draft> listDrafts(const QString& root)
{
    std::vector<Draft> drafts;
    const std::optional<QString> folder = resolveProjectPath(root, draftsFolder);
    if (!folder || !QFileInfo(*folder).isDir())
        return drafts;
    const QDir top(*folder);
    QDirIterator it(*folder, {QStringLiteral("*.toml")}, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString relative = top.relativeFilePath(file);
        const std::optional<QByteArray> toml = readProjectFile(root, draftsFolder + u'/' + relative);
        if (!toml)
            continue;
        std::optional<Draft> draft = readDraftToml(*toml, relative);
        if (!draft)
            continue;
        const std::optional<QByteArray> text = readProjectFile(root, draftPath(draft->path));
        if (!text)
            continue;
        draft->text = *text;
        drafts.push_back(std::move(*draft));
    }
    std::sort(drafts.begin(), drafts.end(), [](const Draft& a, const Draft& b) { return a.path < b.path; });
    return drafts;
}

} // namespace loom
