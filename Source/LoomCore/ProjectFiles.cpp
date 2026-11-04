#include "LoomCore/ProjectFiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

#include <filesystem>
#include <system_error>

namespace loom {

namespace {

/// Whether `path` is `folder` or lies under it. Both are canonical paths with `/`.
/// Windows file names ignore case, so the comparison does too there.
bool isInside(const QString& path, const QString& folder)
{
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    if (path.compare(folder, sensitivity) == 0)
        return true;
    const QString prefix = folder.endsWith(u'/') ? folder : folder + u'/';
    return path.startsWith(prefix, sensitivity);
}

/// The canonical form of an existing path: absolute, with every symbolic link and junction
/// resolved, with `/`. QFileInfo::canonicalFilePath() isn't enough: on Windows it resolves no
/// junction at all (Wiki/gotchas/qt/canonical-path-ignores-junctions.md), while
/// std::filesystem::canonical() asks Windows for the final path.
/// @returns The canonical path, or an empty string if the path doesn't exist or a link in it
///          leads nowhere. No side effects.
QString canonicalPath(const QString& path)
{
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::canonical(QFileInfo(path).filesystemAbsoluteFilePath(), error);
    if (error)
        return {};
    return QDir::cleanPath(QFileInfo(canonical).absoluteFilePath());
}

} // namespace

bool isProjectPath(const QString& relative)
{
    if (relative.isEmpty() || relative.contains(u'\\') || relative.contains(u':'))
        return false;
    const QStringList segments = relative.split(u'/');
    for (const QString& segment : segments) {
        if (segment.isEmpty() || segment == u"." || segment == u"..")
            return false;
    }
    return true;
}

std::optional<QString> resolveProjectPath(const QString& root, const QString& relative)
{
    if (!isProjectPath(relative))
        return std::nullopt;

    const QFileInfo rootInfo(root);
    const QString canonicalRoot = canonicalPath(root);
    if (canonicalRoot.isEmpty() || !rootInfo.isDir())
        return std::nullopt;

    const QString absoluteRoot = QDir::cleanPath(rootInfo.absoluteFilePath());
    const QStringList segments = relative.split(u'/');

    // Look at the path itself, then each folder above it, down to the project folder, and
    // canonicalise the first one that exists: that resolves any link on the way.
    for (qsizetype count = segments.size(); count >= 0; --count) {
        const QString probe = count == 0
            ? absoluteRoot
            : absoluteRoot + u'/' + segments.first(count).join(u'/');
        const QFileInfo info(probe);
        if (!info.exists()) {
            if (info.isSymbolicLink() || info.isJunction())
                return std::nullopt; // A link to nowhere.
            continue;
        }
        const QString canonical = canonicalPath(probe);
        if (canonical.isEmpty() || !isInside(canonical, canonicalRoot))
            return std::nullopt;
        return absoluteRoot + u'/' + relative;
    }
    return std::nullopt; // Unreachable: the project folder exists.
}

std::optional<QByteArray> readProjectFile(const QString& root, const QString& relative)
{
    const std::optional<QString> path = resolveProjectPath(root, relative);
    if (!path)
        return std::nullopt;
    QFile file(*path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return file.readAll();
}

QString writeProjectFile(const QString& root, const QString& relative, const QByteArray& bytes)
{
    const std::optional<QString> path = resolveProjectPath(root, relative);
    if (!path)
        return QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.")
            .arg(relative);

    const QString folder = QFileInfo(*path).absolutePath();
    if (!QDir().mkpath(folder))
        return QStringLiteral("Loom couldn't create the folder `%1`.").arg(QDir::toNativeSeparators(folder));

    QSaveFile file(*path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return QStringLiteral("Loom couldn't write `%1`: %2.")
            .arg(QDir::toNativeSeparators(*path), file.errorString());
    return {};
}

} // namespace loom
