#include "LoomApp/AppSettings.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QStyleHints>

#include <filesystem>
#include <system_error>

namespace loom {

namespace {

// The keys in Loom.ini.
const QString themeKey = QStringLiteral("appearance/theme");
const QString interfaceFontKey = QStringLiteral("appearance/interfaceFontSize");
const QString editorFontKey = QStringLiteral("editor/fontFamily");
const QString editorSizeKey = QStringLiteral("editor/fontSize");
const QString compileOnSaveKey = QStringLiteral("compile/onSave");
const QString compileTimeoutKey = QStringLiteral("compile/timeoutSeconds");
const QString compileModeKey = QStringLiteral("compile/defaultMode");
const QString showTitlesKey = QStringLiteral("ids/showTitles");
const QString reopenKey = QStringLiteral("start/reopenLastProject");
const QString latexmkKey = QStringLiteral("tools/latexmk");
const QString synctexKey = QStringLiteral("tools/synctex");
const QString gitKey = QStringLiteral("tools/git");
const QString recentKey = QStringLiteral("recentProjects");
const QString lastProjectKey = QStringLiteral("state/lastProject");
const QString lastTabKey = QStringLiteral("state/lastTab");
const QString geometryKey = QStringLiteral("window/geometry");

/// Whether two project paths name the same folder, as the file system compares names.
bool samePath(const QString& a, const QString& b)
{
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

} // namespace

AppPaths AppPaths::standard()
{
    AppPaths paths;
    // QSettings' own user-scope INI file for organisation "Loom", application "Loom": on Windows
    // %APPDATA%\Loom\Loom.ini, as the spec says.
    paths.settingsFile = QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Loom"), QStringLiteral("Loom")).fileName();
    // GenericDataLocation is %LOCALAPPDATA% on Windows.
    paths.projectStateFolder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Loom/projects");
    return paths;
}

QString projectStateFile(const AppPaths& paths, const QString& root)
{
    const QString absolute = QFileInfo(root).absoluteFilePath();
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::canonical(QFileInfo(absolute).filesystemFilePath(), error);
    const QString key = error ? QDir::cleanPath(absolute) : QString::fromStdU16String(canonical.generic_u16string());
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex();
    return paths.projectStateFolder + u'/' + QString::fromLatin1(hash.left(12)) + QStringLiteral(".ini");
}

AppSettings::AppSettings(AppPaths paths, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_settings(m_paths.settingsFile, QSettings::IniFormat)
    , m_systemFont(QApplication::font())
{
}

void AppSettings::write(const QString& key, const QVariant& value)
{
    if (m_settings.contains(key) && m_settings.value(key) == value)
        return;
    m_settings.setValue(key, value);
    m_settings.sync();
    emit changed();
}

Theme AppSettings::theme() const
{
    const QString value = m_settings.value(themeKey).toString();
    if (value == u"light")
        return Theme::Light;
    if (value == u"dark")
        return Theme::Dark;
    return Theme::FollowWindows;
}

void AppSettings::setTheme(Theme theme)
{
    const char* name = theme == Theme::Light ? "light" : theme == Theme::Dark ? "dark" : "follow-windows";
    write(themeKey, QString::fromLatin1(name));
}

QString AppSettings::editorFontFamily() const
{
    const QString set = m_settings.value(editorFontKey).toString();
    if (!set.isEmpty())
        return set;
    for (const QString& family : {QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas")}) {
        if (QFontDatabase::hasFamily(family))
            return family;
    }
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

void AppSettings::setEditorFontFamily(const QString& family) { write(editorFontKey, family); }

int AppSettings::editorFontSize() const
{
    const int points = m_settings.value(editorSizeKey, 11).toInt();
    return points > 0 ? points : 11;
}

void AppSettings::setEditorFontSize(int points) { write(editorSizeKey, points); }

int AppSettings::interfaceFontSize() const { return qMax(0, m_settings.value(interfaceFontKey, 0).toInt()); }

void AppSettings::setInterfaceFontSize(int points) { write(interfaceFontKey, qMax(0, points)); }

bool AppSettings::compileOnSave() const { return m_settings.value(compileOnSaveKey, true).toBool(); }

void AppSettings::setCompileOnSave(bool on) { write(compileOnSaveKey, on); }

int AppSettings::compileTimeoutSeconds() const
{
    return qBound(minCompileTimeout, m_settings.value(compileTimeoutKey, 120).toInt(), maxCompileTimeout);
}

void AppSettings::setCompileTimeoutSeconds(int seconds)
{
    write(compileTimeoutKey, qBound(minCompileTimeout, seconds, maxCompileTimeout));
}

BuildMode AppSettings::defaultCompileMode() const
{
    return buildModeFromName(m_settings.value(compileModeKey).toString()).value_or(BuildMode::Alone);
}

void AppSettings::setDefaultCompileMode(BuildMode mode) { write(compileModeKey, buildModeName(mode)); }

bool AppSettings::showTitlesBesideIds() const { return m_settings.value(showTitlesKey, true).toBool(); }

void AppSettings::setShowTitlesBesideIds(bool on) { write(showTitlesKey, on); }

bool AppSettings::reopenLastProject() const { return m_settings.value(reopenKey, true).toBool(); }

void AppSettings::setReopenLastProject(bool on) { write(reopenKey, on); }

QString AppSettings::latexmkPath() const { return m_settings.value(latexmkKey).toString(); }

void AppSettings::setLatexmkPath(const QString& path) { write(latexmkKey, path.trimmed()); }

QString AppSettings::synctexPath() const { return m_settings.value(synctexKey).toString(); }

void AppSettings::setSynctexPath(const QString& path) { write(synctexKey, path.trimmed()); }

QString AppSettings::gitPath() const { return m_settings.value(gitKey).toString(); }

void AppSettings::setGitPath(const QString& path) { write(gitKey, path.trimmed()); }

QString AppSettings::latexmkProgram() const
{
    const QString path = latexmkPath();
    return path.isEmpty() ? QStringLiteral("latexmk") : path;
}

QString AppSettings::synctexProgram() const
{
    const QString path = synctexPath();
    return path.isEmpty() ? QStringLiteral("synctex") : path;
}

GitTool AppSettings::gitTool() const
{
    GitTool git;
    if (const QString path = gitPath(); !path.isEmpty())
        git.program = path;
    return git;
}

std::vector<RecentProject> AppSettings::recentProjects() const
{
    std::vector<RecentProject> recent;
    const int count = m_settings.beginReadArray(recentKey);
    for (int i = 0; i < count && static_cast<int>(recent.size()) < maxRecentProjects; ++i) {
        m_settings.setArrayIndex(i);
        RecentProject project{m_settings.value(QStringLiteral("path")).toString(), m_settings.value(QStringLiteral("name")).toString()};
        if (!project.path.isEmpty())
            recent.push_back(std::move(project));
    }
    m_settings.endArray();
    return recent;
}

/// Writes the whole list, as QSettings arrays are written.
static void writeRecent(QSettings& settings, const QString& key, const std::vector<RecentProject>& recent)
{
    settings.remove(key);
    settings.beginWriteArray(key, static_cast<int>(recent.size()));
    for (int i = 0; i < static_cast<int>(recent.size()); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("path"), recent[static_cast<std::size_t>(i)].path);
        settings.setValue(QStringLiteral("name"), recent[static_cast<std::size_t>(i)].name);
    }
    settings.endArray();
    settings.sync();
}

void AppSettings::addRecentProject(const QString& path, const QString& name)
{
    std::vector<RecentProject> recent = recentProjects();
    std::erase_if(recent, [&](const RecentProject& project) { return samePath(project.path, path); });
    recent.insert(recent.begin(), RecentProject{path, name});
    if (static_cast<int>(recent.size()) > maxRecentProjects)
        recent.resize(maxRecentProjects);
    writeRecent(m_settings, recentKey, recent);
    emit changed();
}

void AppSettings::removeRecentProject(const QString& path)
{
    std::vector<RecentProject> recent = recentProjects();
    std::erase_if(recent, [&](const RecentProject& project) { return samePath(project.path, path); });
    writeRecent(m_settings, recentKey, recent);
    emit changed();
}

QString AppSettings::lastProject() const { return m_settings.value(lastProjectKey).toString(); }

void AppSettings::setLastProject(const QString& path) { write(lastProjectKey, path); }

QString AppSettings::lastTab() const { return m_settings.value(lastTabKey).toString(); }

void AppSettings::setLastTab(const QString& key) { write(lastTabKey, key); }

QByteArray AppSettings::windowGeometry() const { return m_settings.value(geometryKey).toByteArray(); }

void AppSettings::setWindowGeometry(const QByteArray& geometry)
{
    // Not a setting the user changes, so no changed() for it.
    m_settings.setValue(geometryKey, geometry);
    m_settings.sync();
}

void AppSettings::applyAppearance() const
{
    switch (theme()) {
    case Theme::FollowWindows:
        QGuiApplication::styleHints()->unsetColorScheme();
        break;
    case Theme::Light:
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Theme::Dark:
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        break;
    }
    QFont font = m_systemFont;
    if (const int points = interfaceFontSize(); points > 0)
        font.setPointSize(points);
    QApplication::setFont(font);
}

} // namespace loom
