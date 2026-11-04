#include "StatementCommands.h"

#include "LoomCore/Edits.h"

namespace cli {

namespace {

/// Checks that exactly one of `--body` and `--body-file` was given.
QString oneBody(const Args& args, const QString& name)
{
    const bool body = args.value(QStringLiteral("--body")).has_value();
    const bool bodyFile = args.value(QStringLiteral("--body-file")).has_value();
    if (body == bodyFile)
        return QStringLiteral("`loom %1` needs either `--body` or `--body-file`.").arg(name);
    return {};
}

/// What an edit prints: the fields it changed, and the proofs whose checks it outdated.
int reportEdit(const loom::ChangeResult& result, const Context& context)
{
    QString text = result.changed.isEmpty()
        ? QStringLiteral("%1: nothing changed.").arg(result.id)
        : QStringLiteral("%1: changed %2.").arg(result.id, result.changed.join(u", "));
    if (!result.outdatedChecks.isEmpty())
        text += QStringLiteral(" Outdated the checks of %1.").arg(listed(result.outdatedChecks));
    return report(result, context.json, text,
                  QStringLiteral("\"id\": %1, \"changed\": %2, \"outdated_checks\": %3")
                      .arg(jsonString(result.id), jsonStringArray(result.changed), jsonStringArray(result.outdatedChecks)));
}

/// The clicks Dakota has given an edit so far.
struct Confirmations {
    /// He confirmed marking the proof `failed`.
    bool failed = false;
    /// The proofs whose checks he confirmed outdating, as he was shown them.
    std::optional<QStringList> outdateChecks;
};

/// Runs an edit, asking for Dakota's clicks at the terminal as the core asks for them, and runs
/// it again with each one given (Specs/m1-core.md, "Dakota's clicks"):
/// - refused `dakota-only` (only edit-proof's `--level failed` gives it): asks to mark the proof
///   failed;
/// - refused `outdates-checks` and `--outdate-checks` was given: asks to outdate the checks of the
///   proofs it names, and passes exactly those as confirmed. If the project changed in between,
///   so the edit would now outdate others, the core refuses it and nothing is written.
/// The core refuses for rules first, so Dakota is never asked about an edit they refuse.
/// @param edit Runs the edit with the confirmations given so far.
/// @returns The exit code, after printing the result.
template <typename Edit>
int editWithConfirmation(const Args& parsed, const Context& context, Edit edit)
{
    Confirmations given;
    loom::ChangeResult result = edit(given);
    int exitCode = exitDone;
    if (result.code == u"dakota-only") {
        if (!requireTerminal(QStringLiteral("mark a proof failed"), context.json, exitCode)
            || !askDakota(QStringLiteral("mark %1 failed").arg(result.id), context.json, exitCode))
            return exitCode;
        given.failed = true;
        result = edit(given);
    }
    if (result.code == u"outdates-checks" && !given.outdateChecks && parsed.flags.contains(QStringLiteral("--outdate-checks"))) {
        if (!requireTerminal(QStringLiteral("outdate checks"), context.json, exitCode)
            || !askDakota(QStringLiteral("change %1, outdating the checks of %2").arg(result.id, listed(result.outdatedChecks)),
                          context.json, exitCode))
            return exitCode;
        given.outdateChecks = result.outdatedChecks;
        result = edit(given);
    }
    return reportEdit(result, context);
}

/// What a creation prints: the new ID, and with `--json` its file too.
int reportNew(const loom::ChangeResult& result, const Context& context)
{
    return report(result, context.json, result.id,
                  QStringLiteral("\"id\": %1, \"path\": %2").arg(jsonString(result.id), jsonString(result.path)));
}

} // namespace

int runNewTag(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("new-tag");
    int exitCode = exitDone;
    const auto started = start(args, name, {1, {"--kind", "--title"}, {}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    if (const QString problem = missing(parsed, name, {"--kind", "--title"}); !problem.isEmpty())
        return usageError(problem, context.json);

    const loom::ChangeResult result = loom::addTag(root, parsed.positional[0], *parsed.value("--kind"), *parsed.value("--title"));
    return report(result, context.json, result.id, QStringLiteral("\"id\": %1").arg(jsonString(result.id)));
}

int runNewStatement(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("new-statement");
    int exitCode = exitDone;
    const auto started = start(args, name,
        {0, {"--kind", "--title", "--body", "--body-file", "--origin"}, {"--tag", "--stated-using"}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    QString problem = missing(parsed, name, {"--kind", "--title"});
    if (problem.isEmpty())
        problem = oneBody(parsed, name);
    if (!problem.isEmpty())
        return usageError(problem, context.json);
    const std::optional<QByteArray> body = readBody(parsed, context.json, exitCode);
    if (!body)
        return exitCode;

    loom::NewStatement request;
    request.kind = *parsed.value("--kind");
    request.title = *parsed.value("--title");
    request.body = *body;
    request.tags = parsed.repeated.value("--tag");
    request.statedUsing = parsed.repeated.value("--stated-using");
    request.origin = parsed.value("--origin").value_or(QString());
    return reportNew(loom::createStatement(root, request), context);
}

int runEditStatement(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("edit-statement");
    int exitCode = exitDone;
    const auto started = start(args, name,
        {1, {"--title", "--kind", "--body-file", "--superseded-by"},
         {"--add-tag", "--remove-tag", "--add-stated-using", "--remove-stated-using"}, {"--verified", "--outdate-checks"}},
        context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    if (parsed.single.isEmpty() && parsed.repeated.isEmpty() && parsed.flags.isEmpty())
        return usageError(QStringLiteral("`loom %1` needs something to change.").arg(name), context.json);

    loom::StatementEdit edit;
    edit.title = parsed.value("--title");
    edit.kind = parsed.value("--kind");
    edit.supersededBy = parsed.value("--superseded-by");
    if (parsed.value("--body-file")) {
        const std::optional<QByteArray> body = readBody(parsed, context.json, exitCode);
        if (!body)
            return exitCode;
        edit.body = *body;
    }
    edit.addTags = parsed.repeated.value("--add-tag");
    edit.removeTags = parsed.repeated.value("--remove-tag");
    edit.addStatedUsing = parsed.repeated.value("--add-stated-using");
    edit.removeStatedUsing = parsed.repeated.value("--remove-stated-using");
    edit.verified = parsed.flags.contains("--verified");
    return editWithConfirmation(parsed, context, [&](const Confirmations& given) {
        edit.outdateChecks = given.outdateChecks;
        return loom::editStatement(root, parsed.positional[0], edit);
    });
}

int runNewProof(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("new-proof");
    int exitCode = exitDone;
    const auto started = start(args, name, {1, {"--level", "--body", "--body-file"}, {"--premise"}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    QString problem = missing(parsed, name, {"--level"});
    if (problem.isEmpty())
        problem = oneBody(parsed, name);
    if (!problem.isEmpty())
        return usageError(problem, context.json);
    const std::optional<QByteArray> body = readBody(parsed, context.json, exitCode);
    if (!body)
        return exitCode;

    loom::NewProof request;
    request.statement = parsed.positional[0];
    request.level = *parsed.value("--level");
    request.body = *body;
    request.premises = parsed.repeated.value("--premise");
    loom::ChangeResult result = loom::createProof(root, request);
    // A failed proof is Dakota's click; the core refuses it last, so he is asked only about a
    // proof the rules allow.
    if (result.code == u"dakota-only") {
        if (!requireTerminal(QStringLiteral("mark a proof failed"), context.json, exitCode)
            || !askDakota(QStringLiteral("create %1 as a failed proof").arg(result.id), context.json, exitCode))
            return exitCode;
        request.failedConfirmed = true;
        result = loom::createProof(root, request);
    }
    return reportNew(result, context);
}

int runEditProof(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("edit-proof");
    int exitCode = exitDone;
    const auto started = start(args, name,
        {1, {"--level", "--body-file"}, {"--add-premise", "--remove-premise"}, {"--verified", "--outdate-checks"}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    if (parsed.single.isEmpty() && parsed.repeated.isEmpty() && parsed.flags.isEmpty())
        return usageError(QStringLiteral("`loom %1` needs something to change.").arg(name), context.json);

    loom::ProofEdit edit;
    edit.level = parsed.value("--level");
    if (parsed.value("--body-file")) {
        const std::optional<QByteArray> body = readBody(parsed, context.json, exitCode);
        if (!body)
            return exitCode;
        edit.body = *body;
    }
    edit.addPremises = parsed.repeated.value("--add-premise");
    edit.removePremises = parsed.repeated.value("--remove-premise");
    edit.verified = parsed.flags.contains("--verified");
    return editWithConfirmation(parsed, context, [&](const Confirmations& given) {
        edit.outdateChecks = given.outdateChecks;
        edit.failedConfirmed = given.failed;
        return loom::editProof(root, parsed.positional[0], edit);
    });
}

} // namespace cli
