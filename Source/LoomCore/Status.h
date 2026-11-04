#pragma once

// The graph of statements and proofs and every statement's computed status (Specs/m1-core.md,
// "How status is computed"): what each proof gives, the proved set, the rule table, the deciding
// proof, what a conditional statement rests on, cycles of premises, and the sentence `loom status`
// explains a status with. Status is a pure function of the files: nothing here is stored, and
// nothing lets anyone set a status.

#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"

#include <QDate>
#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

namespace loom {

/// A statement's status (Specs/m1-core.md, "How status is computed").
enum class Status {
    None,        ///< Rule 1: a definition, question, remark or example.
    Cited,       ///< Rule 2: a stub checked against the paper's PDF.
    Extracted,   ///< Rule 3: a stub not yet checked.
    False,       ///< Rule 4: refuted, and Dakota accepted it.
    Proved,      ///< Rule 5: in the proved set.
    Conditional, ///< Rule 6: complete and checked, resting on premises that aren't proved or cited.
    Unchecked,   ///< Rule 7: complete, not checked.
    Sketch,      ///< Rule 8.
    Heuristic,   ///< Rule 9.
    Open,        ///< Rule 10: no proof supports it.
};

/// The status as files and JSON name it: `proved`, `conditional`, …; empty for Status::None.
/// No side effects.
QString statusName(Status status);

/// Every status, in the order `loom status` counts them: proved, conditional, unchecked, sketch,
/// heuristic, open, cited, extracted, false, none.
const std::vector<Status>& statusOrder();

/// What one proof gives its statement (Specs/m1-core.md, "What a proof gives").
enum class Gives {
    Nothing,           ///< A `failed` proof, or a counting check is `fail`.
    CompleteChecked,
    CompleteUnchecked,
    Sketch,
    Heuristic,
};

/// `nothing`, `complete-checked`, `complete-unchecked`, `sketch` or `heuristic`. No side effects.
QString givesName(Gives gives);

/// A proof in the graph.
struct ProofNode {
    QString id;           ///< `<tag>.<letters>`, from its file name.
    QString proves;       ///< The tag of its folder: the statement it proves.
    QString path;         ///< Its file, project-relative.
    QStringList premises; ///< As its file lists them.
    QString level;        ///< As its file says; empty if Loom couldn't read it.
    /// Its fingerprint (empty if it can't be computed), check history and which checks count.
    ProofChecking checking;
    Gives gives = Gives::Nothing;
};

/// A statement or paper stub in the graph, with its status.
struct StatementNode {
    QString id;               ///< The tag its file name gives.
    QString kind;
    QString title;            ///< For a stub without a title, its gist.
    QString path;             ///< Its file, project-relative.
    QString paper;            ///< For a stub, its paper's key; empty for a statement.
    QStringList tags;
    QStringList statedUsing;
    QStringList paperPremises; ///< Stubs only.
    QString supersededBy;
    QString refutedBy;
    QDate refutedOn;
    QStringList proofs;       ///< The IDs of its proofs, lowest letter first.

    Status status = Status::None;
    /// The proof the matching rule rests on (proved, conditional, unchecked, sketch, heuristic);
    /// empty for the other statuses. For a proved statement, the proof that brought it into the
    /// proved set; otherwise the spec's tie-break: the fewest premises neither proved nor cited,
    /// then the lowest letter.
    QString decidingProof;
    /// For a conditional statement: the premises of its deciding proof that are neither proved
    /// nor cited, in tag order. Empty otherwise.
    QStringList restsOn;
    /// One sentence saying why it has its status, as `loom status <id>` prints it.
    QString explanation;
};

/// The whole graph, every status computed.
struct ProjectGraph {
    /// Every statement and stub Loom could read, in tag order.
    std::vector<StatementNode> statements;
    /// Every proof Loom could read, by statement tag, then lowest letter first.
    std::vector<ProofNode> proofs;
    /// `stated_using` edges `{from, to}` whose ends are both in `statements`, in tag then file order.
    std::vector<std::pair<QString, QString>> statedUsing;
    /// `paper_premises` edges between stubs, likewise.
    std::vector<std::pair<QString, QString>> paperPremises;
    /// Each cycle of premises as its statements in order, starting at its smallest tag and
    /// without repeating it: `["H8KD", "J3RW"]`.
    std::vector<QStringList> cycles;

    /// The statement or stub `tag`, or null.
    const StatementNode* statement(const QString& tag) const;
    /// The proof `id`, or null.
    const ProofNode* proof(const QString& id) const;
    /// How many statements have `status`.
    int count(Status status) const;
};

/// Builds the graph and computes every status (Specs/m1-core.md, "How status is computed").
/// Rules, first match wins: 1 no status for definitions, questions, remarks and examples; 2 a
/// stub whose `extracted_from` locator is checked against the PDF is cited; 3 another stub is
/// extracted; 4 `refuted_by` and `refuted_on` both set is false; 5 in the proved set is proved;
/// 6 some proof gives complete-checked is conditional; 7 complete-unchecked is unchecked; 8 sketch;
/// 9 heuristic; 10 otherwise open. The proved set is the smallest set closed under "a provable,
/// unrefuted statement with a complete-checked proof whose premises are all proved or cited",
/// computed by adding statements until nothing changes, so a cycle proves nothing by itself.
/// Statuses are recomputed from scratch on every call: a change anywhere reaches everything
/// downstream.
/// @returns The graph. No side effects.
ProjectGraph buildGraph(const ProjectContents& contents);

/// The cycles of premises (Specs/m1-core.md, "Cycles"): S has a proof with premise T, T has a
/// proof with premise S, possibly through more statements. Every proof counts, whatever its
/// level, as the spec words it. A premise naming the statement its proof proves is
/// `premise-self`, not a cycle.
/// @returns Each cycle as findCycles() gives it, ending by repeating its start:
///          `["H8KD", "J3RW", "H8KD"]`. No side effects.
std::vector<QStringList> premiseCycles(const ProjectContents& contents);

} // namespace loom
