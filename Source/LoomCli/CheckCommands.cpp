#include "CheckCommands.h"

#include "LoomCore/Checks.h"
#include "LoomCore/EditSession.h"
#include "LoomCore/Edits.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/TomlWriter.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace cli {

namespace {

/// A check as a JSON object, its fields as in the file, absent ones `null`. No side effects.
QString jsonCheck(const loom::Check& check)
{
    return QStringLiteral("{\"by\": %1, \"date\": %2, \"fingerprint\": %3, \"verdict\": %4, \"changes\": %5, "
                          "\"record\": %6, \"note\": %7}")
        .arg(jsonString(check.by), jsonString(loom::tomlOffsetDateTime(check.date)), jsonString(check.fingerprint),
             jsonString(check.verdict), jsonOptional(check.changes), jsonOptional(check.record), jsonOptional(check.note));
}

/// Before Dakota types a message or `yes`, checks what Loom can without the write lock, so a
/// mistyped ID or verdict fails before it costs him anything (CLAUDE.md, "Never lose user
/// text"): the proof exists, the verdict is one of the three, the proof has a fingerprint, and
/// its check history can be appended to. addCheck() checks all of it again under the lock.
/// @param exitCode Set to the exit code when it refuses, after reporting why.
/// @returns True if the check can go ahead as far as Loom can tell. Reads the project.
bool checkBeforeAsking(const QString& root, const QString& typed, const loom::NewCheck& request, bool json, int& exitCode)
{
    const std::optional<loom::ProjectContents> contents = readProject(root, json, exitCode);
    if (!contents)
        return false;
    const std::optional<QString> id = loom::resolveProof(*contents, typed);
    if (!id) {
        exitCode = fail(exitRefused, QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(typed), json);
        return false;
    }
    if (!loom::checkVerdicts.contains(request.verdict)) {
        exitCode = fail(exitRefused, QStringLiteral("check-verdict"),
            QStringLiteral("`%1` isn't a verdict. Use pass, needs-changes or fail.").arg(request.verdict), json);
        return false;
    }
    QString why;
    if (!loom::proofFingerprint(*contents, *id, &why)) {
        exitCode = fail(exitRefused, QStringLiteral("no-fingerprint"), why, json);
        return false;
    }
    if (const loom::ChecksFile* history = contents->checksOf(*id); history && !history->reading.parsed) {
        exitCode = report(loom::edits::notEditable(history->path, history->reading.problems), json, {}, {});
        return false;
    }
    return true;
}

/// Prints the message Dakota typed back to him on stderr when it wasn't saved, so he can paste
/// it again (CLAUDE.md, "Never lose user text"). Prints nothing for an empty message.
void giveBack(const QString& changes)
{
    if (!changes.trimmed().isEmpty())
        write(stderr, QStringLiteral("Your message wasn't saved. Here it is, to paste again:\n%1\n").arg(changes));
}

} // namespace

int runFingerprint(const QStringList& args, const Context& context)
{
    int exitCode = exitDone;
    const auto started = start(args, QStringLiteral("fingerprint"), {1, {}, {}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    // Reading only, so no write lock.
    const std::optional<loom::ProjectContents> contents = readProject(root, context.json, exitCode);
    if (!contents)
        return exitCode;

    const QString& typed = parsed.positional[0];
    const std::optional<QString> id = loom::resolveProof(*contents, typed);
    if (!id)
        return fail(exitRefused, QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(typed), context.json);
    QString error;
    const std::optional<QString> fingerprint = loom::proofFingerprint(*contents, *id, &error);
    if (!fingerprint)
        return fail(exitRefused, QStringLiteral("no-fingerprint"), error, context.json);
    write(stdout, context.json ? QStringLiteral("{\"proof\": %1, \"fingerprint\": %2}\n").arg(jsonString(*id), jsonString(*fingerprint))
                               : *fingerprint + u'\n');
    return exitDone;
}

int runAddCheck(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("add-check");
    QString error;
    const std::optional<Args> parsedArgs = parseArgs(args, name,
        {1, {"--by", "--verdict", "--changes", "--changes-file", "--fingerprint", "--record", "--note"}, {}, {}}, error);
    if (!parsedArgs)
        return usageError(error, context.json);
    const Args& parsed = *parsedArgs;
    // Wrong usage is reported as such, even outside a project.
    if (const QString problem = missing(parsed, name, {"--by", "--verdict"}); !problem.isEmpty())
        return usageError(problem, context.json);
    if (parsed.value("--changes") && parsed.value("--changes-file"))
        return usageError(QStringLiteral("`loom %1` takes `--changes` or `--changes-file`, not both.").arg(name), context.json);
    const std::optional<QString> project = findProject(context, error);
    if (!project)
        return fail(exitCantWork, QStringLiteral("not-a-project"), error, context.json);
    const QString& root = *project;
    int exitCode = exitDone;

    loom::NewCheck request;
    request.by = *parsed.value("--by");
    request.verdict = *parsed.value("--verdict");
    request.changes = parsed.value("--changes").value_or(QString());
    request.fingerprint = parsed.value("--fingerprint").value_or(QString());
    request.record = parsed.value("--record").value_or(QString());
    request.note = parsed.value("--note").value_or(QString());
    if (const std::optional<QString> path = parsed.value("--changes-file")) {
        const std::optional<QByteArray> bytes = readTextFile(*path, context.json, exitCode);
        if (!bytes)
            return exitCode;
        request.changes = QString::fromUtf8(*bytes);
    }

    const QString& typed = parsed.positional[0];
    bool confirmed = false;
    bool typedAtTerminal = false;
    if (request.by == u"dakota") {
        if (!checkBeforeAsking(root, typed, request, context.json, exitCode)
            || !requireTerminal(QStringLiteral("add Dakota's checks"), context.json, exitCode))
            return exitCode;
        if (request.verdict == u"needs-changes" && request.changes.trimmed().isEmpty()) {
            write(stderr, QStringLiteral("What needs to change in %1? Type it, then an empty line:\n").arg(typed));
            QStringList lines;
            while (const std::optional<QString> line = readTerminalLine()) {
                if (line->isEmpty())
                    break;
                lines << *line;
            }
            request.changes = lines.join(u'\n');
            typedAtTerminal = true;
        }
        if (!askDakota(QStringLiteral("add Dakota's %1 check of %2").arg(request.verdict, typed), context.json, exitCode)) {
            if (typedAtTerminal)
                giveBack(request.changes);
            return exitCode;
        }
        confirmed = true;
    }

    const loom::CheckAdded added = loom::addCheck(root, typed, request, confirmed);
    if (added.change.outcome != loom::ChangeOutcome::Done && typedAtTerminal) {
        // Printed after the reason, so the message is the last thing on the screen.
        const int refusedCode = report(added.change, context.json, {}, {});
        giveBack(request.changes);
        return refusedCode;
    }
    const loom::ChangeResult& result = added.change;
    const loom::Check& check = added.check;
    QString text = QStringLiteral("%1: added check %2, %3's %4 at %5 (fingerprint %6).\n")
                       .arg(result.id, QString::number(added.index + 1), check.by, check.verdict,
                            loom::tomlOffsetDateTime(check.date), check.fingerprint);
    if (!added.standing.supports)
        text += QStringLiteral("A counting check is a fail: %1 doesn't support its statement.").arg(result.id);
    else if (added.standing.checked)
        text += QStringLiteral("%1 is now checked.").arg(result.id);
    else
        text += QStringLiteral("%1 isn't checked.").arg(result.id);
    const QString json =
        QStringLiteral("\"proof\": %1, \"path\": %2, \"index\": %3, \"check\": %4, \"checked\": %5, \"supports\": %6, "
                       "\"counting_agent_check\": %7, \"counting_dakota_check\": %8")
            .arg(jsonString(result.id), jsonString(result.path), QString::number(added.index), jsonCheck(check),
                 added.standing.checked ? QStringLiteral("true") : QStringLiteral("false"),
                 added.standing.supports ? QStringLiteral("true") : QStringLiteral("false"),
                 jsonIndex(added.standing.countingAgent), jsonIndex(added.standing.countingDakota));
    return report(result, context.json, text, json);
}

int runMergeChecks(const QStringList& args, const Context& context)
{
    QString error;
    const std::optional<Args> parsed = parseArgs(args, QStringLiteral("merge-checks"), {3, {}, {}, {}}, error);
    if (!parsed)
        return usageError(error, context.json);

    // Git hands the driver three temporary files, which aren't project files: `ours` is where it
    // wants the result (Specs/m1-core.md, "The project folder", `merge.loom-checks.driver`).
    QByteArray versions[3];
    for (int i = 0; i < 3; ++i) {
        QFile file(parsed->positional[i]);
        if (!file.open(QIODevice::ReadOnly))
            return fail(exitCantWork, QStringLiteral("unreadable"),
                QStringLiteral("Loom couldn't read `%1`.").arg(QDir::toNativeSeparators(parsed->positional[i])), context.json);
        versions[i] = file.readAll();
    }
    const loom::ChecksMerge merge = loom::mergeChecks(versions[0], versions[1], versions[2]);
    if (!merge.ok)
        return fail(exitRefused, QStringLiteral("merge-failed"), merge.message, context.json);

    const QString& ours = parsed->positional[1];
    QSaveFile out(ours);
    if (!out.open(QIODevice::WriteOnly) || out.write(merge.merged) != merge.merged.size() || !out.commit())
        return fail(exitCantWork, QStringLiteral("write-failed"),
            QStringLiteral("Loom couldn't write `%1`.").arg(QDir::toNativeSeparators(ours)), context.json);
    if (merge.restored)
        write(stderr, QStringLiteral("A check was deleted on one side of the merge; Loom kept it, since checks are never removed.\n"));
    return exitDone;
}

} // namespace cli
