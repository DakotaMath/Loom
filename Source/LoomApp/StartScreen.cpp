#include "LoomApp/StartScreen.h"

#include "LoomApp/AppSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace loom {

StartScreen::StartScreen(AppSettings* settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_recent(new QListWidget(this))
{
    setObjectName(QStringLiteral("startScreen"));

    auto* title = new QLabel(QStringLiteral("Loom"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 2);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* newProject = new QPushButton(QStringLiteral("New project…"), this);
    newProject->setObjectName(QStringLiteral("newProject"));
    auto* openProject = new QPushButton(QStringLiteral("Open project…"), this);
    openProject->setObjectName(QStringLiteral("openProject"));
    auto* checkSetup = new QPushButton(QStringLiteral("Check my setup"), this);
    checkSetup->setObjectName(QStringLiteral("checkSetup"));
    connect(newProject, &QPushButton::clicked, this, &StartScreen::newProjectClicked);
    connect(openProject, &QPushButton::clicked, this, &StartScreen::openProjectClicked);
    connect(checkSetup, &QPushButton::clicked, this, &StartScreen::checkSetupClicked);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(newProject);
    buttons->addWidget(openProject);
    buttons->addStretch(1);
    buttons->addWidget(checkSetup);

    auto* recentTitle = new QLabel(QStringLiteral("Recent projects"), this);
    QFont bold = recentTitle->font();
    bold.setBold(true);
    recentTitle->setFont(bold);
    m_recent->setObjectName(QStringLiteral("recentProjects"));
    m_recent->setSelectionMode(QAbstractItemView::SingleSelection);
    const auto open = [this](QListWidgetItem* item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (QFileInfo(path).isDir())
            emit recentClicked(path);
    };
    connect(m_recent, &QListWidget::itemClicked, this, open);
    connect(m_recent, &QListWidget::itemActivated, this, open);

    auto* column = new QVBoxLayout;
    column->addWidget(title);
    column->addSpacing(12);
    column->addLayout(buttons);
    column->addSpacing(18);
    column->addWidget(recentTitle);
    column->addWidget(m_recent, 1);

    auto* outer = new QHBoxLayout(this);
    outer->addStretch(1);
    outer->addLayout(column, 3);
    outer->addStretch(1);
    outer->setContentsMargins(24, 36, 24, 24);

    connect(m_settings, &AppSettings::changed, this, &StartScreen::fillRecent);
    fillRecent();
}

void StartScreen::fillRecent()
{
    m_recent->clear();
    for (const RecentProject& project : m_settings->recentProjects()) {
        const bool exists = QFileInfo(project.path).isDir();
        auto* item = new QListWidgetItem(m_recent);
        item->setData(Qt::UserRole, project.path);
        // The row is drawn by its widget; an item text would show beneath it.
        item->setData(Qt::AccessibleTextRole, project.name);

        auto* row = new QWidget;
        auto* name = new QLabel(project.name, row);
        name->setTextFormat(Qt::PlainText);
        name->setObjectName(QStringLiteral("name"));
        QFont bold = name->font();
        bold.setBold(true);
        name->setFont(bold);
        auto* path = new QLabel(QDir::toNativeSeparators(project.path), row);
        path->setTextFormat(Qt::PlainText);
        path->setObjectName(QStringLiteral("path"));
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        text->addWidget(name);
        text->addWidget(path);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(6, 4, 6, 4);
        layout->addLayout(text, 1);
        if (!exists) {
            // Greyed: drawn as disabled, but the button must still work.
            name->setEnabled(false);
            path->setEnabled(false);
            row->setToolTip(QStringLiteral("This folder no longer exists."));
            auto* remove = new QPushButton(QStringLiteral("Remove from list"), row);
            remove->setObjectName(QStringLiteral("removeFromList"));
            connect(remove, &QPushButton::clicked, this, [this, where = project.path] {
                // Removing rebuilds the list, which deletes this button: do it after the click.
                QMetaObject::invokeMethod(this, [this, where] { m_settings->removeRecentProject(where); }, Qt::QueuedConnection);
            });
            layout->addWidget(remove);
        }
        item->setSizeHint(row->sizeHint());
        m_recent->setItemWidget(item, row);
    }
}

} // namespace loom
