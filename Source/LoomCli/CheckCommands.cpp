#include "CheckCommands.h"

#include "LoomCore/Checks.h"
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

/// A JSON number for an index, or `null`. No side effects.
QString jsonIndex(const std::optional<qsizetype>& index)
{
    return index ? QString::number(*index) : QStringLiteral("null");
}

/// A JSON string, or `null` for an empty one. No side effects.
QString jsonOptional(const QString& text)
{
    return text.isEmpty() ? QStringLiteral("null") : jsonString(text);
}

/// A check as a JSON object, its fields as in the file, absent ones `null`. No side effects.
QString jsonCheck(const loom::Check& check)
{
    return QStringLiteral("{\"by\": %1, \"date\": %2, \"fingerprint\": %3, \"verdict\": %4, \"changes\": %5, "
                          "\"record\": %6, \"note\": %7}")
        .arg(jsonString(check.by), jsonString(loom::tomlOffsetDateTime(check.date)), jsonString(check.fingerprint),
             jsonString(check.verdict), jsonOptional(check.changes), jsonOptional(check.record), jsonOptional(check.note));
}

} // namespace

int runFingerprint(const QStringList& args, const Context& context)
{
    QString error;
    const std::optional<Args> parsed = parseArgs(args, QStringLiteral("fingerprint"), {1, {}, {}, {}}, error);
    if (!parsed)
        return usageError(error, context.json);
    const std::optional<QString> root = findProject(context, error);
    if (!root)
        return fail(exitCantWork, QStringLiteral("not-a-project"), error, context.json);

    // Reading only, so no write lock.
    const loom::OpenedProject project = loom::openProject(*root);
    switch (project.status) {
    case loom::OpenStatus::Opened: break;
    case loom::OpenStatus::NotAProject: return fail(exitCantWork, QStringLiteral("not-a-project"), project.message, context.json);
    case loom::OpenStatus::Unreadable: return fail(exitCantWork, QStringLiteral("unreadable"), project.message, context.json);
    case loom::OpenStatus::FormatTooNew: return fail(exitCantWork, QStringLiteral("format-too-new"), project.message, context.json);
    }
    const loom::ContentsLoading loading = loom::loadContents(project.root);
    if (!loading.ok)
        return fail(exitCantWork, QStringLiteral("unreadable"), loading.message, context.json);

    const QString& typed = parsed->positional[0];
    const std::optional<QString> id = loom::resolveProof(loading.contents, typed);
    if (!id)
        return fail(exitRefused, QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(typed), context.json);
    const std::optional<QString> fingerprint = loom::proofFingerprint(loading.contents, *id, &error);
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
    const std::optional<Args> parsed = parseArgs(args, name,
        {1, {"--by", "--verdict", "--changes", "--changes-file", "--fingerprint", "--record", "--note"}, {}, {}}, error);
    if (!parsed)
        return usageError(error, context.json);
    for (const char* option : {"--by", "--verdict"}) {
        if (!parsed->value(option))
            return usageError(QStringLiteral("`loom %1` needs `%2`.").arg(name, QString::fromLatin1(option)), context.json);
    }
    if (parsed->value("--changes") && parsed->value("--changes-file"))
        return usageError(QStringLiteral("`loom %1` takes `--changes` or `--changes-file`, not both.").arg(name), context.json);
    const std::optional<QString> root = findProject(context, error);
    if (!root)
        return fail(exitCantWork, QStringLiteral("not-a-project"), error, context.json);

    loom::NewCheck request;
    request.by = *parsed->value("--by");
    request.verdict = *parsed->value("--verdict");
    request.changes = parsed->value("--changes").value_or(QString());
    request.fingerprint = parsed->value("--fingerprint").value_or(QString());
    request.record = parsed->value("--record").value_or(QString());
    request.note = parsed->value("--note").value_or(QString());
    int exitCode = exitDone;
    if (const std::optional<QString> path = parsed->value("--changes-file")) {
        const std::optional<QByteArray> bytes = readTextFile(*path, context.json, exitCode);
        if (!bytes)
            return exitCode;
        request.changes = QString::fromUtf8(*bytes);
    }

    const QString& typed = parsed->positional[0];
    bool confirmed = false;
    if (request.by == u"dakota") {
        if (!requireTerminal(QStringLiteral("add Dakota's checks"), context.json, exitCode))
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
        }
        if (!askDakota(QStringLiteral("add Dakota's %1 check of %2").arg(request.verdict, typed), context.json, exitCode))
            return exitCode;
        confirmed = true;
    }

    const loom::CheckAdded added = loom::addCheck(*root, typed, request, confirmed);
    const loom::ChangeResult& result = added.change;
    const loom::Check& check = added.check;
    QString text = QStringLiteral("%1: added check %2, %3's %4 at %5 (fingerprint %6).\n")
                       .arg(result.id).arg(added.index + 1).arg(check.by, check.verdict, loom::tomlOffsetDateTime(check.date), check.fingerprint);
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
