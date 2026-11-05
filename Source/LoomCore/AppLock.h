#pragma once

// One window, one project (Specs/m2-statements-review.md, "Decisions this spec makes": "A project
// opens in one Loom window at a time (`.loom/app.lock`)"). The lock is a file in the project, so
// LoomCore takes it, as it makes every other write there.

#include <QLockFile>
#include <QString>

#include <memory>

namespace loom {

/// How taking the lock went.
enum class AppLockOutcome {
    Taken, ///< This window holds it now.
    Held,  ///< Another Loom window (in this process or another) holds it.
    Failed ///< The lock file couldn't be made: the path, permissions, a disk error.
};

/// The result of takeAppLock().
struct AppLockResult {
    AppLockOutcome outcome = AppLockOutcome::Failed;
    /// For Taken: the lock, released when this is destroyed.
    std::unique_ptr<QLockFile> lock;
    /// For Held and Failed: what to tell the user, in plain English.
    QString message;
};

/// Takes `.loom/app.lock` for a window that opens the project, without waiting.
/// Rules (Specs/m2-statements-review.md, "Opening a folder"): a lock held by a running Loom is
/// *"This project is open in another Loom window."*; a lock left by a window that crashed is stale
/// and taken over, which QLockFile decides from the process that wrote it. Its age never makes it
/// stale (`setStaleLockTime(0)`), since a window may stay open for days.
/// @param root The project folder.
/// @returns The lock or why not. Side effects: creates `.loom/` and `.loom/app.lock`; removes a
///          stale lock file.
AppLockResult takeAppLock(const QString& root);

} // namespace loom
