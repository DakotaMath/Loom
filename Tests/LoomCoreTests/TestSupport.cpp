#include "TestSupport.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <system_error>

namespace loomtest {

void writeFile(const QString& path, const QByteArray& bytes)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

QStringList filesUnder(const QString& folder)
{
    QStringList files;
    const QDir dir(folder);
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString relative = dir.relativeFilePath(it.next());
        if (relative != u".git" && !relative.startsWith(u".git/"))
            files << relative;
    }
    files.sort();
    return files;
}

loom::GitTool testGit(const QString& globalConfig, bool withIdentity)
{
    writeFile(globalConfig, withIdentity ? QByteArray("[user]\n\tname = Loom Test\n\temail = test@example.com\n") : QByteArray());
    loom::GitTool git;
    git.environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), globalConfig);
    git.environment.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    for (const char* name : {"GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_COMMITTER_NAME", "GIT_COMMITTER_EMAIL"})
        git.environment.remove(QString::fromLatin1(name));
    return git;
}

QString gitOutput(const loom::GitTool& git, const QStringList& arguments, const QString& workingDir)
{
    const loom::GitRun run = loom::runGit(git, arguments, workingDir);
    INFO("git " << arguments.join(u' ').toStdString() << ": " << run.errors.toStdString());
    REQUIRE(run.ok());
    return run.output.trimmed();
}

bool makeDirectoryLink(const QString& target, const QString& link)
{
    std::error_code error;
    std::filesystem::create_directory_symlink(target.toStdWString(), link.toStdWString(), error);
    if (!error)
        return true;
#ifdef Q_OS_WIN
    // Junctions need no privileges, and mklink is a cmd built-in.
    return QProcess::execute(QStringLiteral("cmd"),
               {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
                QDir::toNativeSeparators(link), QDir::toNativeSeparators(target)}) == 0;
#else
    return false;
#endif
}

} // namespace loomtest
