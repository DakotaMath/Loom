#include "StatusCommands.h"

#include "LoomCore/Edits.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/Status.h"

#include <algorithm>
#include <optional>

namespace cli {

namespace {

/// A status for JSON: its name, or `null` for kinds without one.
QString jsonStatus(loom::Status status)
{
    return status == loom::Status::None ? QStringLiteral("null") : jsonString(loom::statusName(status));
}

/// A boolean for JSON.
QString jsonBool(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

/// One element of `loom status --json`'s `statements` (Specs/m1-core.md, "JSON shapes").
QString jsonStatement(const loom::StatementNode& node)
{
    return QStringLiteral("{\"id\": %1, \"kind\": %2, \"title\": %3, \"paper\": %4, \"status\": %5, \"deciding_proof\": %6, "
                          "\"rests_on\": %7, \"superseded_by\": %8, \"explanation\": %9}")
        .arg(jsonString(node.id), jsonString(node.kind), jsonString(node.title), jsonOptional(node.paper),
             jsonStatus(node.status), jsonOptional(node.decidingProof), jsonStringArray(node.restsOn),
             jsonOptional(node.supersededBy), jsonString(node.explanation));
}

/// The counts by status, as `loom status --json` gives them: every status, `none` last.
QString jsonCounts(const loom::ProjectGraph& graph)
{
    QStringList counts;
    for (const loom::Status status : loom::statusOrder()) {
        const QString name = status == loom::Status::None ? QStringLiteral("none") : loom::statusName(status);
        counts << QStringLiteral("%1: %2").arg(jsonString(name)).arg(graph.count(status));
    }
    return u'{' + counts.join(u", ") + u'}';
}

/// "27 statements: 8 proved, 6 conditional, …, 2 without a status." Statuses no statement has
/// are left out.
QString textCounts(const loom::ProjectGraph& graph)
{
    if (graph.statements.empty())
        return QStringLiteral("No statements yet.");
    QStringList counts;
    for (const loom::Status status : loom::statusOrder()) {
        if (const int count = graph.count(status); count > 0)
            counts << QStringLiteral("%1 %2").arg(count).arg(status == loom::Status::None ? QStringLiteral("without a status")
                                                                                           : loom::statusName(status));
    }
    const qsizetype total = static_cast<qsizetype>(graph.statements.size());
    return QStringLiteral("%1 %2: %3.").arg(total).arg(total == 1 ? QStringLiteral("statement") : QStringLiteral("statements"),
                                                        counts.join(u", "));
}

} // namespace

int runStatus(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("status");
    int exitCode = exitDone;
    // `[<id>]`: the ID is optional, so take exactly as many positionals as were given (none or one).
    const qsizetype positionals = args.isEmpty() || args[0].startsWith(u"--") ? 0 : 1;
    const auto started = start(args, name, {positionals, {}, {}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    const std::optional<loom::ProjectContents> contents = readProject(root, context.json, exitCode);
    if (!contents)
        return exitCode;
    const loom::ProjectGraph graph = loom::buildGraph(*contents);

    if (!parsed.positional.isEmpty()) {
        const QString& typed = parsed.positional[0];
        const std::optional<QString> tag = loom::resolveStatement(*contents, typed);
        const loom::StatementNode* node = tag ? graph.statement(*tag) : nullptr;
        if (!node)
            return fail(exitRefused, QStringLiteral("ref-unknown"),
                tag ? QStringLiteral("Loom can't read %1's file, so it has no status. `loom validate` says why.").arg(*tag)
                    : QStringLiteral("`%1` isn't a statement in this project.").arg(typed),
                context.json);
        write(stdout, (context.json ? jsonStatement(*node) : node->explanation) + u'\n');
        return exitDone;
    }

    if (context.json) {
        QStringList statements;
        for (const loom::StatementNode& node : graph.statements)
            statements << jsonStatement(node);
        write(stdout, QStringLiteral("{\"statements\": [%1], \"counts\": %2}\n").arg(statements.join(u", "), jsonCounts(graph)));
        return exitDone;
    }
    qsizetype width = 0;
    for (const loom::StatementNode& node : graph.statements)
        width = std::max(width, loom::statusName(node.status).size());
    QString text;
    for (const loom::StatementNode& node : graph.statements) {
        const QString status = node.status == loom::Status::None ? QStringLiteral("-") : loom::statusName(node.status);
        text += QStringLiteral("%1  %2  %3\n").arg(node.id, status.leftJustified(width), node.title);
    }
    write(stdout, text + textCounts(graph) + u'\n');
    return exitDone;
}

int runGraph(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("graph");
    if (!context.json)
        return usageError(QStringLiteral("`loom %1` needs `--json`: the graph is for programs; `loom status` is for reading.").arg(name),
                          context.json);
    int exitCode = exitDone;
    const auto started = start(args, name, {0, {}, {}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const std::optional<loom::ProjectContents> contents = readProject(started->second, context.json, exitCode);
    if (!contents)
        return exitCode;
    const loom::ProjectGraph graph = loom::buildGraph(*contents);

    QStringList nodes;
    for (const loom::StatementNode& node : graph.statements)
        nodes << QStringLiteral("{\"id\": %1, \"kind\": %2, \"title\": %3, \"status\": %4, \"paper\": %5, \"superseded_by\": %6, \"tags\": %7}")
                     .arg(jsonString(node.id), jsonString(node.kind), jsonString(node.title), jsonStatus(node.status),
                          jsonOptional(node.paper), jsonOptional(node.supersededBy), jsonStringArray(node.tags));
    QStringList proofs;
    for (const loom::ProofNode& proof : graph.proofs) {
        const loom::CheckStanding& standing = proof.checking.standing;
        // One .arg() call for all ten: a second call would replace a `%1` inside the values the
        // first put in, such as a hand-edited level, and could break the JSON.
        proofs << QStringLiteral("{\"id\": %1, \"proves\": %2, \"premises\": %3, \"level\": %4, \"fingerprint\": %5, \"checked\": %6, "
                                 "\"supports\": %7, \"gives\": %8, \"counting_agent_check\": %9, \"counting_dakota_check\": %10}")
                      .arg(jsonString(proof.id), jsonString(proof.proves), jsonStringArray(proof.premises), jsonOptional(proof.level),
                           jsonOptional(proof.checking.fingerprint), jsonBool(standing.checked), jsonBool(standing.supports),
                           jsonString(loom::givesName(proof.gives)), jsonIndex(standing.countingAgent),
                           jsonIndex(standing.countingDakota));
    }
    const auto edges = [](const std::vector<std::pair<QString, QString>>& list) {
        QStringList out;
        for (const auto& [from, to] : list)
            out << QStringLiteral("{\"from\": %1, \"to\": %2}").arg(jsonString(from), jsonString(to));
        return u'[' + out.join(u", ") + u']';
    };
    QStringList cycles;
    for (const QStringList& cycle : graph.cycles)
        cycles << jsonStringArray(cycle);

    write(stdout, QStringLiteral("{\"format\": 1, \"nodes\": [%1], \"proofs\": [%2], \"stated_using\": %3, \"paper_premises\": %4, "
                                 "\"cycles\": [%5]}\n")
                      .arg(nodes.join(u", "), proofs.join(u", "), edges(graph.statedUsing), edges(graph.paperPremises),
                           cycles.join(u", ")));
    return exitDone;
}

int runRefute(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("refute");
    int exitCode = exitDone;
    const auto started = start(args, name, {1, {"--by"}, {}, {}}, context, exitCode, QStringLiteral("a statement's ID"));
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    if (const QString problem = missing(parsed, name, {"--by"}); !problem.isEmpty())
        return usageError(problem, context.json);

    const QString& typed = parsed.positional[0];
    const QString proof = *parsed.value("--by");
    if (!requireTerminal(QStringLiteral("refute a statement"), context.json, exitCode)
        || !askDakota(QStringLiteral("mark %1 false, refuted by %2").arg(typed, proof), context.json, exitCode))
        return exitCode;

    const loom::ChangeResult result = loom::refuteStatement(root, typed, proof, true);
    if (result.outcome != loom::ChangeOutcome::Done)
        return report(result, context.json, {}, {});

    // "Prints: the new status", computed from the files as they now are.
    const std::optional<loom::ProjectContents> contents = readProject(root, context.json, exitCode);
    if (!contents)
        return exitCode;
    const loom::ProjectGraph graph = loom::buildGraph(*contents);
    const loom::StatementNode* node = graph.statement(result.id);
    const QString explanation = node ? node->explanation : QString();
    return report(result, context.json, explanation,
                  QStringLiteral("\"id\": %1, \"path\": %2, \"status\": %3, \"explanation\": %4")
                      .arg(jsonString(result.id), jsonString(result.path), node ? jsonStatus(node->status) : QStringLiteral("null"),
                           jsonString(explanation)));
}

} // namespace cli
