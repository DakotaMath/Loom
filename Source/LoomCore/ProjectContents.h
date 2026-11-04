#pragma once

// The statements, proofs and papers of a project, read from `Statements/`, `Proofs/` and
// `Papers/`, and the problems that need more than one file to see (Specs/m1-core.md,
// "Statements", "Proofs", "IDs", "Papers" and "Validation"), with each proof's check history
// ("Checks"). Paper stubs share the statements' tags: one namespace.

#include "LoomCore/Checks.h"
#include "LoomCore/Papers.h"
#include "LoomCore/Problem.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/StatementsAndProofs.h"

#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace loom {

/// A statement file and what reading it gave.
struct StatementFile {
    QString path;             ///< `Statements/<tag>.tex`
    QString tag;              ///< The tag its file name gives; the project refers to it by this.
    StatementReading reading;
};

/// A proof file and what reading it gave.
struct ProofFile {
    QString path;             ///< `Proofs/<tag>/<tag>.<letters>.tex`
    QString folder;           ///< The `<tag>` of its folder.
    QString id;               ///< The proof ID its file name gives, `<tag>.<letters>`.
    ProofReading reading;
};

/// A verbose proof file and what reading it gave.
struct VerboseFile {
    QString path;             ///< `Proofs/<tag>/<proof-id>.verbose.tex`
    QString proofId;          ///< The proof its file name gives.
    VerboseReading reading;
};

/// A check history file and what reading it gave.
struct ChecksFile {
    QString path;             ///< `Proofs/<tag>/<proof-id>.checks.toml`
    QString proofId;          ///< The proof its file name gives.
    ChecksReading reading;
};

/// A paper entry and what reading it gave.
struct PaperFile {
    QString path;             ///< `Papers/<Key>/paper.toml`
    QString folder;           ///< The `<Key>` of its folder; the project refers to the paper by this.
    PaperReading reading;
    /// The folders in its `Sessions/`, as readings name them: `Sessions/<name>`.
    QStringList sessions;
};

/// A paper stub and what reading it gave.
struct StubFile {
    QString path;             ///< `Papers/<Key>/Statements/<tag>.toml`
    QString paperFolder;      ///< The `<Key>` of its paper's folder.
    QString tag;              ///< The tag its file name gives.
    StubReading reading;
};

/// Everything in `Statements/`, `Proofs/` and `Papers/`, each list sorted by path.
struct ProjectContents {
    std::vector<StatementFile> statements;
    std::vector<ProofFile> proofs;
    std::vector<VerboseFile> verbose;
    std::vector<ChecksFile> checks;
    std::vector<PaperFile> papers;
    std::vector<StubFile> stubs;
    /// Problems found while listing the folders: `stray-file`, a paper folder without its
    /// `paper.toml` (`missing-field`), and `bad-path` for a file that resolves outside the project.
    std::vector<Problem> listingProblems;

    /// The statement whose file name gives `tag`, or null. Stubs aren't statements: see stub().
    const StatementFile* statement(const QString& tag) const;
    /// The stub whose file name gives `tag`, or null.
    const StubFile* stub(const QString& tag) const;
    /// The paper in the folder `Papers/<folder>/`, or null.
    const PaperFile* paper(const QString& folder) const;
    /// The paper whose folder, key or old key is `key`, or null.
    const PaperFile* paperWithKey(const QString& key) const;
    /// The proof whose file name gives `id`, or null.
    const ProofFile* proof(const QString& id) const;
    /// The check history of the proof `id`, or null if it has none yet.
    const ChecksFile* checksOf(const QString& id) const;
    /// The statement or stub tag that has `alias` among its aliases, or an empty string.
    QString tagWithAlias(const QString& alias) const;
};

/// The result of loadContents().
struct ContentsLoading {
    /// False if a file couldn't be read at all (permissions, a disk error); then `message` says which.
    bool ok = false;
    ProjectContents contents;
    QString message;
};

/// Reads every statement, proof, verbose proof, check history, paper entry and stub of a project
/// (Specs/m1-core.md, "Statements", "Proofs" and "Papers"). A missing `Statements/`, `Proofs/` or
/// `Papers/` folder is the same as an empty one.
/// Rules for what may sit where: `Statements/` holds only `<tag>.tex`; `Proofs/` holds only
/// folders named by a tag; `Proofs/<tag>/` holds only `<tag>.<letters>.tex`, `.verbose.tex` and
/// `.checks.toml`; `Papers/` holds only folders, each `paper.toml`, `Statements/` (only
/// `<tag>.toml`), and `notes.md`, `Sessions/` and `local/`, which M1 doesn't read. Anything else
/// is a `stray-file`, and isn't read.
/// @param root The project folder.
/// @returns The files and what reading each gave. Reads files; writes nothing.
ContentsLoading loadContents(const QString& root);

/// Every problem in the project that statements and proofs show: each file's own problems,
/// the listing's, and those that need several files (`tag-unknown`, `id-syntax` and `ref-alias`
/// in references, `id-duplicate`, `alias-duplicate`, `ref-unknown`, `stated-using-self`,
/// `stated-using-cycle`, `superseded-invalid`, `refuted-unknown`, `refuted-no-status`,
/// `proof-folder-unknown`, `proves-mismatch`, `proof-of-no-status`, `premise-no-status`, `premise-cycle`,
/// `verbose-orphan`, `checks-orphan`), with each check history's own problems; and for papers
/// and stubs, each file's own problems and `paper-key-mismatch`, `paper-key-rule` (a warning),
/// `paper-key-duplicate`, `session-missing` (a warning), `version-unknown`, `stub-paper-premise`
/// and `proof-of-stub`. Stubs join statements in every check of tags, aliases and references.
/// The warnings that need statuses, and bodies, are checked by later issues.
/// @param config   The project's `loom.toml`, for its tags.
/// @param contents The statements and proofs.
/// @returns The problems, sorted with sortProblems(). No side effects.
std::vector<Problem> checkContents(const ProjectConfig& config, const ProjectContents& contents);

/// Every tag a new statement or stub mustn't take (Specs/m1-core.md, "IDs", "Generation"): the
/// tags and aliases of every statement and stub, and every proof folder's tag, so a new
/// statement never adopts proofs left behind in a folder.
/// @returns The tags and aliases. No side effects.
QSet<QString> tagsInUse(const ProjectContents& contents);

/// Reads a statement or stub ID a person typed (Specs/m1-core.md, "IDs": tags are forgiving on
/// the command line, and aliases are resolved wherever a person types an ID).
/// @param typed E.g. `4kx7`, `4KX7` or `thm:energy-decay`.
/// @returns The tag of an existing statement or stub, or nothing. No side effects.
std::optional<QString> resolveStatement(const ProjectContents& contents, const QString& typed);

/// Reads a proof ID a person typed, as resolveStatement() reads the tag part.
/// @param typed E.g. `4kx7.A`.
/// @returns The ID of an existing proof, or nothing. No side effects.
std::optional<QString> resolveProof(const ProjectContents& contents, const QString& typed);

} // namespace loom
