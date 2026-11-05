#include "LoomCore/AppLock.h"

#include "LoomCore/ProjectFiles.h"

#include <QDir>
#include <QFileInfo>

namespace loom {

AppLockResult takeAppLock(const QString& root)
{
    AppLockResult result;
    const QString relative = QStringLiteral(".loom/app.lock");
    const std::optional<QString> path = resolveProjectPath(root, relative);
    if (!path) {
        result.message = QStringLiteral("Loom can't make `%1`: it would be outside the project.").arg(relative);
        return result;
    }
    if (!QDir().mkpath(QFileInfo(*path).absolutePath())) {
        result.message = QStringLiteral("Loom couldn't create the folder `.loom` in `%1`.").arg(QDir::toNativeSeparators(root));
        return result;
    }

    auto lock = std::make_unique<QLockFile>(*path);
    lock->setStaleLockTime(0);
    if (lock->tryLock(0)) {
        result.outcome = AppLockOutcome::Taken;
        result.lock = std::move(lock);
        return result;
    }
    switch (lock->error()) {
    case QLockFile::LockFailedError:
        result.outcome = AppLockOutcome::Held;
        result.message = QStringLiteral("This project is open in another Loom window.");
        break;
    case QLockFile::PermissionError:
        result.message = QStringLiteral("Loom isn't allowed to write `%1`, so it can't open the project.")
                             .arg(QDir::toNativeSeparators(*path));
        break;
    case QLockFile::NoError:
    case QLockFile::UnknownError:
        result.message = QStringLiteral("Loom couldn't write `%1`, so it can't open the project.").arg(QDir::toNativeSeparators(*path));
        break;
    }
    return result;
}

} // namespace loom
