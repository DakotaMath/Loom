#include "LoomCore/Status.h"

#include "LoomCore/Cycles.h"
#include "LoomCore/Ids.h"

#include <QHash>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <limits>
#include <utility>

namespace loom {

namespace {

/// "A, B and C". No side effects.
QString listed(const QStringList& items)
{
    if (items.size() < 2)
        return items.join(QString());
    return items.first(items.size() - 1).join(u", ") + QStringLiteral(" and ") + items.last();
}

/// Orders proof IDs by statement tag, then lowest letter first (`z` before `aa`). No side effects.
bool proofBefore(const QString& a, const QString& b)
{
    const std::optional<ProofIdParts> pa = splitProofId(a);
    const std::optional<ProofIdParts> pb = splitProofId(b);
    if (!pa || !pb)
        return a < b;
    if (pa->tag != pb->tag)
        return pa->tag < pb->tag;
    return proofNumber(pa->letters) < proofNumber(pb->letters);
}

/// What a proof gives (Specs/m1-core.md, "What a proof gives"): nothing if it is `failed` or
/// doesn't support its statement (a counting check is `fail`, "Checks" rule 7), else its level,
/// with `complete` split by whether it is checked. A level Loom doesn't know gives nothing.
Gives givesOf(const QString& level, const CheckStanding& standing)
{
    if (level == u"failed" || !standing.supports)
        return Gives::Nothing;
    if (level == u"complete")
        return standing.checked ? Gives::CompleteChecked : Gives::CompleteUnchecked;
    if (level == u"sketch")
        return Gives::Sketch;
    if (level == u"heuristic")
        return Gives::Heuristic;
    return Gives::Nothing;
}

/// Computes statuses and explanations over a graph whose nodes and proofs are filled in.
class Computation {
public:
    /// Indexes the graph's nodes and proofs, and lists each statement's proofs, lowest letter
    /// first (the proofs arrive sorted).
    /// @param graph    The graph to compute; run() writes its statuses.
    /// @param locators Each stub's locator for its `extracted_from` version, for explanations.
    Computation(ProjectGraph& graph, QHash<QString, Locator> locators) : m_graph(graph), m_locators(std::move(locators))
    {
        for (StatementNode& node : m_graph.statements)
            m_nodes.insert(node.id, &node);
        for (ProofNode& proof : m_graph.proofs) {
            m_proofs.insert(proof.id, &proof);
            if (StatementNode* node = m_nodes.value(proof.proves))
                node->proofs << proof.id;
        }
    }

    /// Computes the proved set, then every statement's status, deciding proof and explanation.
    /// Side effects: writes them into the graph.
    void run()
    {
        computeProvedSet();
        for (StatementNode& node : m_graph.statements)
            decide(node);
        for (StatementNode& node : m_graph.statements)
            node.explanation = explain(node);
    }

private:
    ProjectGraph& m_graph;
    QHash<QString, Locator> m_locators;
    QHash<QString, StatementNode*> m_nodes;
    QHash<QString, ProofNode*> m_proofs;
    /// The proved set: each statement in it, to the proof that brought it in.
    QHash<QString, QString> m_proved;

    /// Whether a node is a paper stub. buildGraph() gives a `paper` to stubs only, and never an
    /// empty one (the key, else the folder's name). No side effects.
    static bool isStub(const StatementNode& node) { return !node.paper.isEmpty(); }

    /// Rule 4: both fields set. One without the other is `refuted-incomplete`, and doesn't count.
    static bool isRefuted(const StatementNode& node) { return !node.refutedBy.isEmpty() && node.refutedOn.isValid(); }

    /// A stub that is cited (rule 2). Only stubs of a provable kind count: rule 1 comes first.
    bool isCitedTag(const QString& tag) const
    {
        const StatementNode* node = m_nodes.value(tag);
        return node && node->status == Status::Cited;
    }

    /// Premises of `proof` that are neither proved nor cited, in tag order.
    QStringList unmet(const ProofNode& proof) const
    {
        QStringList result;
        for (const QString& premise : proof.premises) {
            if (!m_proved.contains(premise) && !isCitedTag(premise) && !result.contains(premise))
                result << premise;
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    /// Rules 1–3, which don't depend on proofs, set first so cited premises are known. A stub
    /// arrives with rule 2 or 3 applied; rule 1 comes before them.
    static void presetStatus(StatementNode& node)
    {
        if (!isProvableKind(node.kind))
            node.status = Status::None;
        else if (!isStub(node))
            node.status = Status::Open;
    }

    /// The proved set (Specs/m1-core.md, "The proved set"): start empty and add every provable,
    /// unrefuted statement (not a stub) with a complete-checked proof whose premises are all
    /// proved or cited, until nothing changes. Being the smallest such set, it never lets a
    /// statement help prove itself.
    /// It grows in rounds, each judged against the set as the round began, so a statement's entry
    /// doesn't depend on the order statements are visited. Each statement keeps the proof that
    /// brought it in, its lowest letter among those that could: that proof's premises were all
    /// proved or cited before it, so the explanation never goes round in a circle (the worked
    /// example's "H8KD proved (via J3RW.b)").
    void computeProvedSet()
    {
        for (StatementNode& node : m_graph.statements)
            presetStatus(node);
        for (;;) {
            QHash<QString, QString> entering;
            for (const StatementNode& node : m_graph.statements) {
                if (m_proved.contains(node.id) || isStub(node) || !isProvableKind(node.kind) || isRefuted(node))
                    continue;
                for (const QString& id : node.proofs) {
                    const ProofNode& proof = *m_proofs.value(id);
                    if (proof.gives == Gives::CompleteChecked && unmet(proof).isEmpty()) {
                        entering.insert(node.id, id);
                        break;
                    }
                }
            }
            if (entering.isEmpty())
                return;
            m_proved.insert(entering);
        }
    }

    /// The deciding proof among those giving `gives` (Specs/m1-core.md, "Tie-breaks"): the
    /// fewest premises that are neither proved nor cited, then the lowest letter. `node.proofs`
    /// is lowest letter first, so the first with the fewest wins.
    QString deciding(const StatementNode& node, Gives gives) const
    {
        QString best;
        qsizetype fewest = std::numeric_limits<qsizetype>::max();
        for (const QString& id : node.proofs) {
            const ProofNode& proof = *m_proofs.value(id);
            if (proof.gives != gives)
                continue;
            const qsizetype count = unmet(proof).size();
            if (count < fewest) {
                fewest = count;
                best = id;
            }
        }
        return best;
    }

    /// Rules 4–10 for a provable statement; rules 1–3 are already set.
    void decide(StatementNode& node) const
    {
        if (!isProvableKind(node.kind) || isStub(node))
            return;
        if (isRefuted(node)) {
            node.status = Status::False;
            return;
        }
        if (m_proved.contains(node.id)) {
            node.status = Status::Proved;
            node.decidingProof = m_proved.value(node.id);
            return;
        }
        const std::pair<Gives, Status> ranks[] = {
            {Gives::CompleteChecked, Status::Conditional},
            {Gives::CompleteUnchecked, Status::Unchecked},
            {Gives::Sketch, Status::Sketch},
            {Gives::Heuristic, Status::Heuristic},
        };
        for (const auto& [gives, status] : ranks) {
            const QString id = deciding(node, gives);
            if (id.isEmpty())
                continue;
            node.status = status;
            node.decidingProof = id;
            if (status == Status::Conditional)
                node.restsOn = unmet(*m_proofs.value(id));
            return;
        }
        node.status = Status::Open;
    }

    /// "is unchecked", "is extracted (not yet checked against the paper's PDF)", "is a definition
    /// and has no status", …: what a premise is, as the rest of "rests on 3WQ6, which …". The
    /// verb is part of it, so that a premise that isn't there reads "which isn't …".
    /// No side effects.
    QString premiseState(const QString& tag) const
    {
        const StatementNode* node = m_nodes.value(tag);
        if (!node)
            return QStringLiteral("isn't a statement in this project");
        switch (node->status) {
        case Status::None: return QStringLiteral("is a %1 and has no status").arg(node->kind);
        case Status::Extracted: return QStringLiteral("is extracted (not yet checked against the paper's PDF)");
        default: return QStringLiteral("is ") + statusName(node->status);
        }
    }

    /// "Dakota asked for changes …", "has an agent pass, but no pass from Dakota", …: what an
    /// unchecked deciding proof lacks (Specs/m1-core.md, "Tie-breaks and explanations"). When
    /// Loom can't compute the proof's fingerprint, no check can match ("Checks", rule 4), and
    /// that is what it lacks: the sentence gives proofFingerprint()'s reason, since the proof
    /// itself may not have changed at all.
    QString lacks(const ProofNode& proof) const
    {
        const ProofChecking& c = proof.checking;
        if (c.fingerprint.isEmpty()) {
            QString why = c.fingerprintProblem.isEmpty() ? QStringLiteral("Loom can't compute its fingerprint") : c.fingerprintProblem;
            while (why.endsWith(u'.'))
                why.chop(1); // explain() ends the sentence.
            return QStringLiteral("%1 is complete, but no check of it can count. %2").arg(proof.id, why);
        }
        const Check* agent = c.standing.countingAgent ? &c.checks[*c.standing.countingAgent] : nullptr;
        const Check* dakota = c.standing.countingDakota ? &c.checks[*c.standing.countingDakota] : nullptr;
        if (dakota && dakota->verdict == u"needs-changes")
            return QStringLiteral("Dakota asked for changes to %1: \"%2\"").arg(proof.id, dakota->changes.simplified());
        if (agent && agent->verdict == u"needs-changes")
            return agent->record.isEmpty()
                ? QStringLiteral("an agent asked for changes to %1").arg(proof.id)
                : QStringLiteral("an agent asked for changes to %1 (see %2)").arg(proof.id, agent->record);
        if (!agent) {
            const bool older = std::any_of(c.checks.begin(), c.checks.end(),
                                           [&](const Check& check) { return check.byAgent() && check.fingerprint != c.fingerprint; });
            return QStringLiteral("%1 is complete, but has no agent pass%2")
                .arg(proof.id, older ? QStringLiteral(" of this version (it changed after its last agent check)") : QString());
        }
        if (!dakota)
            return QStringLiteral("%1 is complete and has an agent pass, but no pass from Dakota").arg(proof.id);
        return QStringLiteral("%1 is complete and has an agent pass, but no pass from Dakota newer than it").arg(proof.id);
    }

    /// "D9XF.a failed", "an agent check says D9XF.b is wrong", …: why a proof gives nothing.
    QString givesNothing(const ProofNode& proof) const
    {
        if (proof.level == u"failed")
            return QStringLiteral("%1 failed").arg(proof.id);
        const ProofChecking& c = proof.checking;
        const auto fails = [&](const std::optional<qsizetype>& index) { return index && c.checks[*index].verdict == u"fail"; };
        if (fails(c.standing.countingDakota))
            return QStringLiteral("Dakota's check says %1 is wrong").arg(proof.id);
        if (fails(c.standing.countingAgent))
            return QStringLiteral("an agent check says %1 is wrong").arg(proof.id);
        return QStringLiteral("%1 has no level Loom knows").arg(proof.id);
    }

    /// The sentence `loom status <id>` prints (Specs/m1-core.md, "`loom status <id>` explains in
    /// one sentence").
    QString explain(const StatementNode& node) const
    {
        const QString head = node.id + u' ' + statusName(node.status) + QStringLiteral(": ");
        const ProofNode* deciding = m_proofs.value(node.decidingProof);
        switch (node.status) {
        case Status::None:
            return QStringLiteral("%1 has no status: it is a %2.").arg(node.id, node.kind.isEmpty() ? QStringLiteral("statement of no known kind") : node.kind);
        case Status::Cited:
        case Status::Extracted:
            return head + stubSentence(node);
        case Status::False:
            return head + QStringLiteral("refuted by %1, accepted %2.").arg(node.refutedBy, node.refutedOn.toString(Qt::ISODate));
        case Status::Proved: {
            QStringList proved;
            QStringList cited;
            for (const QString& premise : sortedPremises(*deciding))
                (isCitedTag(premise) ? cited : proved) << premise;
            QString premises;
            if (proved.isEmpty() && cited.isEmpty())
                premises = QStringLiteral("and has no premises");
            else if (cited.isEmpty())
                premises = proved.size() == 1 ? QStringLiteral("and its premise %1 is proved").arg(proved[0])
                                              : QStringLiteral("and its premises %1 are proved").arg(proved.join(u", "));
            else if (proved.isEmpty())
                premises = cited.size() == 1 ? QStringLiteral("and its premise %1 is cited").arg(cited[0])
                                             : QStringLiteral("and its premises %1 are cited").arg(cited.join(u", "));
            else
                premises = QStringLiteral("and its premises are proved (%1) or cited (%2)").arg(proved.join(u", "), cited.join(u", "));
            return head + QStringLiteral("%1 is complete and checked, %2.").arg(deciding->id, premises);
        }
        case Status::Conditional: {
            QStringList parts;
            for (const QString& tag : node.restsOn)
                parts << QStringLiteral("%1, which %2").arg(tag, premiseState(tag));
            return head + QStringLiteral("%1 is complete and checked, but rests on %2.").arg(deciding->id, listed(parts));
        }
        case Status::Unchecked: {
            // A quoted `changes` ends the sentence itself, as in the spec's example.
            const QString why = lacks(*deciding);
            return head + why + (why.endsWith(u'"') ? QString() : QStringLiteral("."));
        }
        case Status::Sketch:
            return head + QStringLiteral("%1 is a sketch.").arg(deciding->id);
        case Status::Heuristic:
            return head + QStringLiteral("%1 is a heuristic argument.").arg(deciding->id);
        case Status::Open: {
            if (node.proofs.isEmpty())
                return head + QStringLiteral("it has no proofs.");
            QStringList why;
            for (const QString& id : node.proofs)
                why << givesNothing(*m_proofs.value(id));
            return head + QStringLiteral("no proof supports it (%1).").arg(why.join(QStringLiteral("; ")));
        }
        }
        return head;
    }

    /// The deciding proof's premises, without repeats, in tag order.
    static QStringList sortedPremises(const ProofNode& proof)
    {
        QStringList premises = proof.premises;
        premises.removeDuplicates();
        std::sort(premises.begin(), premises.end());
        return premises;
    }

    /// "Theorem 1.2, p. 4 in SmithJones24 (arxiv-v1), checked against the PDF on 2026-10-18."
    QString stubSentence(const StatementNode& node) const
    {
        const Locator locator = m_locators.value(node.id);
        const QString where = locator.at.isEmpty() ? QStringLiteral("a result of %1").arg(node.paper)
                                                   : QStringLiteral("%1 in %2 (%3)").arg(locator.at, node.paper, locator.version);
        if (node.status == Status::Cited)
            return QStringLiteral("%1, checked against the PDF on %2.").arg(where, locator.checkedAgainstPdf.toString(Qt::ISODate));
        return QStringLiteral("%1, not yet checked against the PDF.").arg(where);
    }
};

} // namespace

QString statusName(Status status)
{
    switch (status) {
    case Status::None: return {};
    case Status::Cited: return QStringLiteral("cited");
    case Status::Extracted: return QStringLiteral("extracted");
    case Status::False: return QStringLiteral("false");
    case Status::Proved: return QStringLiteral("proved");
    case Status::Conditional: return QStringLiteral("conditional");
    case Status::Unchecked: return QStringLiteral("unchecked");
    case Status::Sketch: return QStringLiteral("sketch");
    case Status::Heuristic: return QStringLiteral("heuristic");
    case Status::Open: return QStringLiteral("open");
    }
    return {};
}

const std::vector<Status>& statusOrder()
{
    static const std::vector<Status> order = {Status::Proved, Status::Conditional, Status::Unchecked, Status::Sketch,
                                              Status::Heuristic, Status::Open, Status::Cited, Status::Extracted,
                                              Status::False, Status::None};
    return order;
}

QString givesName(Gives gives)
{
    switch (gives) {
    case Gives::Nothing: return QStringLiteral("nothing");
    case Gives::CompleteChecked: return QStringLiteral("complete-checked");
    case Gives::CompleteUnchecked: return QStringLiteral("complete-unchecked");
    case Gives::Sketch: return QStringLiteral("sketch");
    case Gives::Heuristic: return QStringLiteral("heuristic");
    }
    return {};
}

const StatementNode* ProjectGraph::statement(const QString& tag) const
{
    const auto it = std::lower_bound(statements.begin(), statements.end(), tag,
                                     [](const StatementNode& node, const QString& t) { return node.id < t; });
    return it != statements.end() && it->id == tag ? &*it : nullptr;
}

const ProofNode* ProjectGraph::proof(const QString& id) const
{
    const auto it = std::find_if(proofs.begin(), proofs.end(), [&](const ProofNode& p) { return p.id == id; });
    return it != proofs.end() ? &*it : nullptr;
}

int ProjectGraph::count(Status status) const
{
    return static_cast<int>(std::count_if(statements.begin(), statements.end(),
                                          [&](const StatementNode& node) { return node.status == status; }));
}

std::vector<QStringList> premiseCycles(const ProjectContents& contents)
{
    QMap<QString, QStringList> premises;
    for (const ProofFile& file : contents.proofs) {
        if (!file.reading.parsed || !contents.statement(file.folder))
            continue;
        for (const QString& premise : file.reading.document.premises) {
            // Stubs have no proofs, so a cycle never passes through one.
            if (contents.statement(premise) && !premises.value(file.folder).contains(premise))
                premises[file.folder] << premise;
        }
    }
    return findCycles(premises);
}

ProjectGraph buildGraph(const ProjectContents& contents)
{
    ProjectGraph graph;
    QHash<QString, Locator> locators;
    QSet<QString> seen;

    // A tag used twice is `id-duplicate`; the statement wins over a stub, then the first by path.
    for (const StatementFile& file : contents.statements) {
        if (!file.reading.parsed || seen.contains(file.tag))
            continue;
        seen.insert(file.tag);
        const Statement& s = file.reading.document;
        StatementNode node;
        node.id = file.tag;
        node.kind = s.kind;
        node.title = s.title;
        node.path = file.path;
        node.tags = s.tags;
        node.statedUsing = s.statedUsing;
        node.supersededBy = s.supersededBy;
        node.refutedBy = s.refutedBy;
        node.refutedOn = s.refutedOn;
        graph.statements.push_back(node);
    }
    // A stub's `paper` is its paper's key (the `loom status` JSON: "`paper` is the key for
    // stubs"), as ContentsChecker names it in messages: the key, else the folder's name when the
    // entry has no key Loom could read. They differ only alongside `paper-key-mismatch`.
    QHash<QString, QString> keyOfFolder;
    for (const PaperFile& file : contents.papers) {
        if (file.reading.parsed && !file.reading.document.key.isEmpty())
            keyOfFolder.insert(file.folder, file.reading.document.key);
    }
    for (const StubFile& file : contents.stubs) {
        if (!file.reading.parsed || seen.contains(file.tag))
            continue;
        seen.insert(file.tag);
        const Stub& s = file.reading.document;
        StatementNode node;
        node.id = file.tag;
        node.kind = s.kind;
        node.title = s.title.isEmpty() ? s.gist : s.title; // "Without one, Loom shows the gist."
        node.path = file.path;
        node.paper = keyOfFolder.value(file.paperFolder, file.paperFolder);
        node.statedUsing = s.statedUsing;
        node.paperPremises = s.paperPremises;
        // Rules 2 and 3; rule 1 (a kind without status) is applied over this by the computation.
        node.status = isCited(s) ? Status::Cited : Status::Extracted;
        if (const Locator* locator = s.locator(s.extractedFrom))
            locators.insert(file.tag, *locator);
        graph.statements.push_back(node);
    }
    std::sort(graph.statements.begin(), graph.statements.end(),
              [](const StatementNode& a, const StatementNode& b) { return a.id < b.id; });

    for (const ProofFile& file : contents.proofs) {
        if (!file.reading.parsed)
            continue;
        const Proof& p = file.reading.document;
        ProofNode proof;
        proof.id = file.id;
        proof.proves = file.folder;
        proof.path = file.path;
        proof.premises = p.premises;
        proof.level = p.level;
        proof.checking = proofChecking(contents, file.id);
        proof.gives = givesOf(p.level, proof.checking.standing);
        graph.proofs.push_back(proof);
    }
    std::sort(graph.proofs.begin(), graph.proofs.end(), [](const ProofNode& a, const ProofNode& b) { return proofBefore(a.id, b.id); });

    for (const StatementNode& node : graph.statements) {
        for (const QString& to : node.statedUsing) {
            if (graph.statement(to))
                graph.statedUsing.emplace_back(node.id, to);
        }
        for (const QString& to : node.paperPremises) {
            if (graph.statement(to))
                graph.paperPremises.emplace_back(node.id, to);
        }
    }
    for (QStringList cycle : premiseCycles(contents)) {
        cycle.removeLast();
        graph.cycles.push_back(cycle);
    }

    Computation(graph, std::move(locators)).run();
    return graph;
}

} // namespace loom
