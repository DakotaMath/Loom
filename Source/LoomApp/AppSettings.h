#pragma once

// The app's settings and what it remembers, per user and never in the project
// (Specs/m2-statements-review.md, "Settings" and "What the app remembers").

#include "LoomCore/BuildDocument.h"
#include "LoomCore/Git.h"

#include <QByteArray>
#include <QFont>
#include <QObject>
#include <QSettings>
#include <QString>

#include <vector>

namespace loom {

/// Where the app keeps its files. Tests point both at a temporary folder.
struct AppPaths {
    /// The settings, the recent projects, the last project and tab, window geometry:
    /// `%APPDATA%\Loom\Loom.ini`.
    QString settingsFile;
    /// One file per project: `%LOCALAPPDATA%\Loom\projects`.
    QString projectStateFolder;

    /// The real locations for this user (on Linux, `~/.config/Loom/Loom.ini` and
    /// `~/.local/share/Loom/projects`). No side effects.
    static AppPaths standard();
};

/// The file that remembers one project's state for this user (Specs/m2-statements-review.md,
/// "What the app remembers").
/// Rule: `<projectStateFolder>/<first 12 hex characters of the SHA-256 of the project's canonical
/// path>.ini`, the path as UTF-8 with `/`, resolved with std::filesystem::canonical(), which (unlike
/// Qt's) resolves junctions and gives Windows' own spelling of each name, so one folder always
/// gives one file.
/// @param paths The app's folders.
/// @param root  The project folder; it should exist (if not, its absolute path is used).
/// @returns The file's path. Reads the disk only.
QString projectStateFile(const AppPaths& paths, const QString& root);

/// The Theme setting.
enum class Theme { FollowWindows, Light, Dark };

/// A line of the start screen's *Recent projects*.
struct RecentProject {
    QString path; ///< The project folder, absolute, with `/`.
    QString name; ///< Its name from `loom.toml` when it was last opened.
};

/// The settings, read and written through QSettings in INI format. Every setter writes at once
/// and emits changed(). One object is shared by every window.
class AppSettings : public QObject
{
    Q_OBJECT

public:
    /// How many recent projects the start screen lists.
    static constexpr int maxRecentProjects = 10;
    /// The compile timeout's range, in seconds (the spec's Notes column: "10–1800").
    static constexpr int minCompileTimeout = 10;
    static constexpr int maxCompileTimeout = 1800;

    /// Opens the settings file (created on the first write).
    /// @param paths Where the files are.
    /// @param parent The Qt parent.
    /// Side effects: remembers QApplication's font as "Windows' size", for the interface font size
    /// setting to return to.
    explicit AppSettings(AppPaths paths, QObject* parent = nullptr);

    /// Where the files are.
    const AppPaths& paths() const { return m_paths; }

    Theme theme() const;
    void setTheme(Theme theme);
    /// The editor's font family: as set, else Cascadia Mono, else Consolas, else the system's
    /// fixed-width font.
    QString editorFontFamily() const;
    void setEditorFontFamily(const QString& family);
    /// The editor's font size in points: 11 by default.
    int editorFontSize() const;
    void setEditorFontSize(int points);
    /// The interface font size in points, or 0 for Windows' own.
    int interfaceFontSize() const;
    void setInterfaceFontSize(int points);
    /// Ctrl+S saves and compiles when on (the default); only saves when off.
    bool compileOnSave() const;
    void setCompileOnSave(bool on);
    /// How long a build may run, in seconds: 120 by default, kept within 10–1800.
    int compileTimeoutSeconds() const;
    void setCompileTimeoutSeconds(int seconds);
    /// The mode a statement opens with the first time: Alone by default.
    BuildMode defaultCompileMode() const;
    void setDefaultCompileMode(BuildMode mode);
    /// Whether the editor shows titles beside IDs: on by default.
    bool showTitlesBesideIds() const;
    void setShowTitlesBesideIds(bool on);
    /// Whether the app reopens the last project at start: on by default.
    bool reopenLastProject() const;
    void setReopenLastProject(bool on);
    /// A full path for `latexmk`, `synctex` or `git`, or empty to find it on PATH.
    QString latexmkPath() const;
    void setLatexmkPath(const QString& path);
    QString synctexPath() const;
    void setSynctexPath(const QString& path);
    QString gitPath() const;
    void setGitPath(const QString& path);

    /// The program to run: the path set, else the name to find on PATH.
    QString latexmkProgram() const;
    QString synctexProgram() const;
    /// The git LoomCore runs: gitPath(), else `git`, with the system environment.
    GitTool gitTool() const;

    /// The recent projects, newest first, at most maxRecentProjects.
    std::vector<RecentProject> recentProjects() const;
    /// Puts a project at the top of the list (moving it if it's there already, matched by path
    /// without regard to case on Windows). Side effects: writes the settings; emits changed().
    void addRecentProject(const QString& path, const QString& name);
    /// Takes a project off the list. Side effects: writes the settings; emits changed().
    void removeRecentProject(const QString& path);

    /// The project open when a window last closed or opened one, or empty.
    QString lastProject() const;
    void setLastProject(const QString& path);
    /// The key of the tab last used (`statements`, `review`, `settings`), or empty.
    QString lastTab() const;
    void setLastTab(const QString& key);
    /// The main window's geometry, as QWidget::saveGeometry() gives it.
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);

    /// Applies the theme and interface font size to the running application.
    /// Side effects: changes QGuiApplication's colour scheme and QApplication's font.
    void applyAppearance() const;

signals:
    /// Any setting changed.
    void changed();

private:
    /// Writes one value and emits changed(), if it differs.
    void write(const QString& key, const QVariant& value);

    AppPaths m_paths;
    mutable QSettings m_settings;
    QFont m_systemFont;
};

} // namespace loom
