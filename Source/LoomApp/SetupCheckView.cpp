#include "LoomApp/SetupCheckView.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/Lane.h"
#include "LoomApp/LineIcons.h"

#include <QCoreApplication>
#include <QDir>
#include <QHeaderView>
#include <QPointer>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace loom {

namespace {

/// The icon for a line's state.
QIcon iconFor(SetupState state)
{
    switch (state) {
    case SetupState::Found:
        return lineIcon(LineIcon::Tick);
    case SetupState::Missing:
        return lineIcon(LineIcon::Cross);
    case SetupState::Warning:
        return lineIcon(LineIcon::Warning);
    case SetupState::Skipped:
        break;
    }
    return lineIcon(LineIcon::Skipped);
}

} // namespace

SetupCheckView::SetupCheckView(AppSettings* settings, Lane* lane, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_lane(lane)
    , m_button(new QPushButton(QStringLiteral("Check my setup"), this))
    , m_results(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("setupCheck"));
    m_button->setObjectName(QStringLiteral("checkSetup"));
    // Wide enough for both of its labels, so it doesn't jump (or clip) as it changes.
    m_button->setText(QStringLiteral("Checking…"));
    const int checkingWidth = m_button->sizeHint().width();
    m_button->setText(QStringLiteral("Check my setup"));
    m_button->setMinimumWidth(qMax(checkingWidth, m_button->sizeHint().width()));
    connect(m_button, &QPushButton::clicked, this, &SetupCheckView::start);

    m_results->setObjectName(QStringLiteral("setupResults"));
    m_results->setColumnCount(2);
    m_results->setHeaderHidden(true);
    m_results->setRootIsDecorated(false);
    m_results->setSelectionMode(QAbstractItemView::NoSelection);
    m_results->setWordWrap(true);
    m_results->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_results->header()->setStretchLastSection(true);
    m_results->setMinimumHeight(240);
    m_results->hide();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout;
    row->addWidget(m_button);
    row->addStretch(1);
    layout->addLayout(row);
    layout->addWidget(m_results, 1);
}

SetupCheckView::~SetupCheckView()
{
    stop();
}

void SetupCheckView::stop()
{
    if (m_stop)
        m_stop->store(true);
}

void SetupCheckView::start()
{
    if (m_running)
        return;
    m_running = true;
    m_results->clear();
    m_results->show();
    m_button->setEnabled(false);
    m_button->setText(QStringLiteral("Checking…"));

    SetupRequest request;
    request.git = m_settings->gitTool().program;
    request.latexmk = m_settings->latexmkProgram();
    request.synctex = m_settings->synctexProgram();
    request.compileTimeoutMs = m_settings->compileTimeoutSeconds() * 1000;
    m_stop = std::make_shared<std::atomic_bool>(false);
    request.stop = m_stop;

    // Each line is shown as soon as it's checked: the test build can take a while.
    QPointer<SetupCheckView> self(this);
    m_lane->run(this,
        [request, self] {
            return checkSetup(request, [self](const SetupItem& item) {
                QMetaObject::invokeMethod(qApp, [self, item] {
                    if (self)
                        self->add(item);
                }, Qt::QueuedConnection);
            });
        },
        [this](const std::vector<SetupItem>&) {
            m_running = false;
            m_button->setEnabled(true);
            m_button->setText(QStringLiteral("Check my setup"));
            emit finished();
        });
}

void SetupCheckView::add(const SetupItem& item)
{
    QString detail;
    switch (item.state) {
    case SetupState::Found:
        detail = item.path.isEmpty() ? item.version : QStringLiteral("%1\n%2").arg(item.version, QDir::toNativeSeparators(item.path));
        break;
    case SetupState::Missing:
    case SetupState::Warning:
    case SetupState::Skipped:
        detail = item.message;
        if (!item.path.isEmpty())
            detail += u'\n' + QDir::toNativeSeparators(item.path);
        break;
    }
    auto* line = new QTreeWidgetItem(m_results, {item.name, detail});
    line->setIcon(0, iconFor(item.state));
    line->setData(0, Qt::UserRole, item.key);
    line->setData(0, Qt::UserRole + 1, static_cast<int>(item.state));
    line->setToolTip(1, detail);
}

} // namespace loom
