// One window, one project: `.loom/app.lock` (Specs/m2-statements-review.md, "Decisions this spec
// makes" and "Opening a folder": "Is open in another Loom window … A window that crashed leaves a
// stale lock, which QLockFile detects and takes over").

#include "LoomCore/AppLock.h"

#include "TestSupport.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("app lock: one holder at a time", "[app-lock]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    loom::AppLockResult first = loom::takeAppLock(temp.path());
    REQUIRE(first.outcome == loom::AppLockOutcome::Taken);
    CHECK(QFileInfo::exists(temp.path() + "/.loom/app.lock"));

    // A second window, here in the same process, can't take it.
    const loom::AppLockResult second = loom::takeAppLock(temp.path());
    CHECK(second.outcome == loom::AppLockOutcome::Held);
    CHECK(second.message == "This project is open in another Loom window.");
    CHECK(!second.lock);

    // Closing the first window frees it.
    first.lock.reset();
    CHECK(loom::takeAppLock(temp.path()).outcome == loom::AppLockOutcome::Taken);
}

TEST_CASE("app lock: a crashed window's lock is taken over", "[app-lock]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    // A process that has ended stands for the window that crashed.
    QProcess ended;
    ended.start(QStringLiteral(LOOM_FAKE_TOOL), {});
    REQUIRE(ended.waitForStarted());
    const qint64 pid = ended.processId();
    REQUIRE(ended.waitForFinished());
    // QLockFile's own layout: the process ID, the program's name, the machine's name. An empty
    // name means this machine (QLockFile's spelling of it differs from QSysInfo's on Windows).
    loomtest::writeFile(temp.path() + "/.loom/app.lock", QByteArray::number(pid) + "\nLoomCoreTests\n\n");
    CHECK(loom::takeAppLock(temp.path()).outcome == loom::AppLockOutcome::Taken);
}

TEST_CASE("app lock: a lock held for days isn't stale", "[app-lock]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const loom::AppLockResult first = loom::takeAppLock(temp.path());
    REQUIRE(first.outcome == loom::AppLockOutcome::Taken);
    QFile file(temp.path() + "/.loom/app.lock");
    // Windows keeps the held lock file open without write sharing, so its time can't be changed
    // there; the age rule matters most where it can.
    if (!file.open(QIODevice::ReadWrite) || !file.setFileTime(QDateTime::currentDateTime().addDays(-3), QFileDevice::FileModificationTime))
        SKIP("The held lock file's time can't be changed here");
    file.close();
    CHECK(loom::takeAppLock(temp.path()).outcome == loom::AppLockOutcome::Held);
}
