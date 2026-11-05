#include "LoomApp/NewProjectDialog.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/Lane.h"

#include "LoomCore/NewProject.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace loom {

NewProjectDialog::NewProjectDialog(AppSettings* settings, Lane* lane, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_lane(lane)
    , m_parentFolder(new QLineEdit(this))
    , m_folderName(new QLineEdit(this))
    , m_name(new QLineEdit(this))
    , m_idea(new QPlainTextEdit(this))
    , m_message(new QLabel(this))
{
    setObjectName(QStringLiteral("newProjectDialog"));
    setWindowTitle(QStringLiteral("New project"));
    setWindowModality(Qt::WindowModal);
    resize(560, 360);

    m_parentFolder->setObjectName(QStringLiteral("parentFolder"));
    m_parentFolder->setText(QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
    m_folderName->setObjectName(QStringLiteral("folderName"));
    m_name->setObjectName(QStringLiteral("projectName"));
    m_idea->setObjectName(QStringLiteral("idea"));
    m_idea->setPlaceholderText(QStringLiteral("Optional: the research idea, in your own words"));
    m_idea->setTabChangesFocus(true);
    m_message->setObjectName(QStringLiteral("message"));
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message->hide();

    auto* browse = new QPushButton(QStringLiteral("Browse…"), this);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Where to create the project"), m_parentFolder->text());
        if (!folder.isEmpty())
            m_parentFolder->setText(QDir::toNativeSeparators(folder));
    });
    auto* parentRow = new QHBoxLayout;
    parentRow->addWidget(m_parentFolder, 1);
    parentRow->addWidget(browse);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("In the folder"), parentRow);
    form->addRow(QStringLiteral("Folder name"), m_folderName);
    form->addRow(QStringLiteral("Project name"), m_name);
    form->addRow(QStringLiteral("Research idea"), m_idea);

    auto* buttons = new QDialogButtonBox(this);
    m_create = buttons->addButton(QStringLiteral("Create"), QDialogButtonBox::AcceptRole);
    m_create->setObjectName(QStringLiteral("create"));
    m_create->setDefault(true);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &NewProjectDialog::create);
    connect(buttons, &QDialogButtonBox::rejected, this, &NewProjectDialog::reject);

    // The folder name follows the project's name until it's typed in itself.
    connect(m_name, &QLineEdit::textEdited, this, [this](const QString& name) {
        if (!m_folderName->isModified())
            m_folderName->setText(name.trimmed());
    });
    for (QLineEdit* edit : {m_parentFolder, m_folderName, m_name})
        connect(edit, &QLineEdit::textChanged, this, &NewProjectDialog::updateCreate);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form, 1);
    layout->addWidget(m_message);
    layout->addWidget(buttons);
    updateCreate();
    m_name->setFocus();
}

void NewProjectDialog::prefill(const QString& parentFolder, const QString& folderName, const QString& projectName)
{
    m_parentFolder->setText(QDir::toNativeSeparators(parentFolder));
    m_folderName->setText(folderName);
    m_folderName->setModified(true);
    m_name->setText(projectName);
}

QString NewProjectDialog::loomBesideApp()
{
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("loom.exe");
#else
    const QString name = QStringLiteral("loom");
#endif
    const QString path = QCoreApplication::applicationDirPath() + u'/' + name;
    return QFileInfo(path).isFile() ? path : QString();
}

void NewProjectDialog::reject()
{
    if (!m_creating)
        QDialog::reject();
}

void NewProjectDialog::updateCreate()
{
    m_create->setEnabled(!m_creating && !m_parentFolder->text().trimmed().isEmpty() && !m_folderName->text().trimmed().isEmpty()
                         && !m_name->text().trimmed().isEmpty());
}

void NewProjectDialog::create()
{
    if (m_creating || !m_create->isEnabled())
        return;
    const QString folderName = m_folderName->text().trimmed();
    if (folderName.contains(u'/') || folderName.contains(u'\\')) {
        m_message->setText(QStringLiteral("The folder name is one folder's name, without `/` or `\\`. Choose where it goes in \"In the folder\"."));
        m_message->show();
        return;
    }

    NewProjectRequest request;
    request.folder = QDir::fromNativeSeparators(m_parentFolder->text().trimmed()) + u'/' + folderName;
    request.name = m_name->text().trimmed();
    request.idea = m_idea->toPlainText().trimmed();
    request.git = m_settings->gitTool();
    request.loomProgram = loomBesideApp();

    m_creating = true;
    m_message->hide();
    m_create->setText(QStringLiteral("Creating…"));
    updateCreate();
    m_lane->run(this, [request] { return createProject(request); }, [this](const NewProjectResult& result) {
        m_creating = false;
        m_create->setText(QStringLiteral("Create"));
        updateCreate();
        if (result.outcome == NewProjectOutcome::Created) {
            emit created(result.path);
            accept();
            return;
        }
        m_message->setText(result.message);
        m_message->show();
    });
}

} // namespace loom
