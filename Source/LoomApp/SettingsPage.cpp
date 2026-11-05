#include "LoomApp/SettingsPage.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/SetupCheckView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace loom {

namespace {

/// A bold heading for a group of settings.
QLabel* heading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.15);
    label->setFont(font);
    return label;
}

} // namespace

SettingsPage::SettingsPage(AppSettings* settings, Lane* lane, QWidget* parent)
    : QWidget(parent)
    , m_setupCheck(nullptr)
{
    setObjectName(QStringLiteral("settingsPage"));
    auto* content = new QWidget;
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // Appearance.
    auto* theme = new QComboBox(content);
    theme->setObjectName(QStringLiteral("theme"));
    theme->addItem(QStringLiteral("Follow Windows"), static_cast<int>(Theme::FollowWindows));
    theme->addItem(QStringLiteral("Light"), static_cast<int>(Theme::Light));
    theme->addItem(QStringLiteral("Dark"), static_cast<int>(Theme::Dark));
    theme->setCurrentIndex(theme->findData(static_cast<int>(settings->theme())));
    connect(theme, &QComboBox::currentIndexChanged, this, [settings, theme] {
        settings->setTheme(static_cast<Theme>(theme->currentData().toInt()));
        settings->applyAppearance();
    });
    form->addRow(QStringLiteral("Theme"), theme);

    auto* editorFont = new QFontComboBox(content);
    editorFont->setObjectName(QStringLiteral("editorFont"));
    editorFont->setFontFilters(QFontComboBox::MonospacedFonts);
    editorFont->setCurrentFont(QFont(settings->editorFontFamily()));
    connect(editorFont, &QFontComboBox::currentFontChanged, this,
            [settings](const QFont& font) { settings->setEditorFontFamily(font.family()); });
    auto* editorSize = new QSpinBox(content);
    editorSize->setObjectName(QStringLiteral("editorFontSize"));
    editorSize->setRange(6, 48);
    editorSize->setSuffix(QStringLiteral(" pt"));
    editorSize->setValue(settings->editorFontSize());
    connect(editorSize, &QSpinBox::valueChanged, this, [settings](int points) { settings->setEditorFontSize(points); });
    auto* editorRow = new QHBoxLayout;
    editorRow->addWidget(editorFont, 1);
    editorRow->addWidget(editorSize);
    form->addRow(QStringLiteral("Editor font"), editorRow);

    auto* interfaceSize = new QComboBox(content);
    interfaceSize->setObjectName(QStringLiteral("interfaceFontSize"));
    interfaceSize->addItem(QStringLiteral("Windows' size"), 0);
    for (int points : {8, 9, 10, 11, 12, 13, 14, 16, 18, 20})
        interfaceSize->addItem(QStringLiteral("%1 pt").arg(points), points);
    const int current = interfaceSize->findData(settings->interfaceFontSize());
    interfaceSize->setCurrentIndex(current < 0 ? 0 : current);
    connect(interfaceSize, &QComboBox::currentIndexChanged, this, [settings, interfaceSize] {
        settings->setInterfaceFontSize(interfaceSize->currentData().toInt());
        settings->applyAppearance();
    });
    form->addRow(QStringLiteral("Interface font size"), interfaceSize);

    // Compiling.
    auto* compileOnSave = new QCheckBox(QStringLiteral("Ctrl+S saves and compiles (off: it only saves)"), content);
    compileOnSave->setObjectName(QStringLiteral("compileOnSave"));
    compileOnSave->setChecked(settings->compileOnSave());
    connect(compileOnSave, &QCheckBox::toggled, this, [settings](bool on) { settings->setCompileOnSave(on); });
    form->addRow(QStringLiteral("Compile on save"), compileOnSave);

    auto* timeout = new QSpinBox(content);
    timeout->setObjectName(QStringLiteral("compileTimeout"));
    timeout->setRange(AppSettings::minCompileTimeout, AppSettings::maxCompileTimeout);
    timeout->setSuffix(QStringLiteral(" seconds"));
    timeout->setValue(settings->compileTimeoutSeconds());
    connect(timeout, &QSpinBox::valueChanged, this, [settings](int seconds) { settings->setCompileTimeoutSeconds(seconds); });
    form->addRow(QStringLiteral("Compile timeout"), timeout);

    auto* mode = new QComboBox(content);
    mode->setObjectName(QStringLiteral("defaultCompileMode"));
    mode->addItem(QStringLiteral("Alone"), static_cast<int>(BuildMode::Alone));
    mode->addItem(QStringLiteral("With premises"), static_cast<int>(BuildMode::Premises));
    mode->addItem(QStringLiteral("With the chain"), static_cast<int>(BuildMode::Chain));
    mode->setCurrentIndex(mode->findData(static_cast<int>(settings->defaultCompileMode())));
    connect(mode, &QComboBox::currentIndexChanged, this,
            [settings, mode] { settings->setDefaultCompileMode(static_cast<BuildMode>(mode->currentData().toInt())); });
    form->addRow(QStringLiteral("Default compile mode"), mode);

    // The editor and starting.
    auto* titles = new QCheckBox(QStringLiteral("Show each ID's title beside it in the editor"), content);
    titles->setObjectName(QStringLiteral("showTitles"));
    titles->setChecked(settings->showTitlesBesideIds());
    connect(titles, &QCheckBox::toggled, this, [settings](bool on) { settings->setShowTitlesBesideIds(on); });
    form->addRow(QStringLiteral("Titles beside IDs"), titles);

    auto* reopen = new QCheckBox(QStringLiteral("Open the last project, on the tab last used, when Loom starts"), content);
    reopen->setObjectName(QStringLiteral("reopenLastProject"));
    reopen->setChecked(settings->reopenLastProject());
    connect(reopen, &QCheckBox::toggled, this, [settings](bool on) { settings->setReopenLastProject(on); });
    form->addRow(QStringLiteral("Reopen the last project"), reopen);

    // The tools: a full path overrides PATH.
    const auto toolRow = [&](const QString& objectName, const QString& tool, const QString& value, auto set) {
        auto* edit = new QLineEdit(value, content);
        edit->setObjectName(objectName);
        edit->setPlaceholderText(QStringLiteral("Found on PATH"));
        edit->setClearButtonEnabled(true);
        connect(edit, &QLineEdit::editingFinished, this, [edit, set] { set(edit->text()); });
        auto* browse = new QPushButton(QStringLiteral("Browse…"), content);
        connect(browse, &QPushButton::clicked, this, [this, edit, set, tool] {
            const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Where is %1?").arg(tool), edit->text());
            if (file.isEmpty())
                return;
            edit->setText(QDir::toNativeSeparators(file));
            set(edit->text());
        });
        auto* row = new QHBoxLayout;
        row->addWidget(edit, 1);
        row->addWidget(browse);
        form->addRow(tool, row);
    };
    toolRow(QStringLiteral("latexmkPath"), QStringLiteral("latexmk"), settings->latexmkPath(),
            [settings](const QString& path) { settings->setLatexmkPath(path); });
    toolRow(QStringLiteral("synctexPath"), QStringLiteral("synctex"), settings->synctexPath(),
            [settings](const QString& path) { settings->setSynctexPath(path); });
    toolRow(QStringLiteral("gitPath"), QStringLiteral("git"), settings->gitPath(),
            [settings](const QString& path) { settings->setGitPath(path); });

    m_setupCheck = new SetupCheckView(settings, lane, content);

    auto* note = new QLabel(QStringLiteral("Settings are yours: Loom keeps them in your user profile, never in a project."), content);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);

    auto* column = new QVBoxLayout(content);
    column->addWidget(heading(QStringLiteral("Settings"), content));
    column->addWidget(note);
    column->addSpacing(8);
    column->addLayout(form);
    column->addSpacing(16);
    column->addWidget(heading(QStringLiteral("Check my setup"), content));
    column->addWidget(m_setupCheck);
    column->addStretch(1);
    column->setContentsMargins(24, 18, 24, 18);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);
}

} // namespace loom
