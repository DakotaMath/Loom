#pragma once

// The statements and proofs of a project, read from `Statements/` and `Proofs/`, and the problems
// that need more than one file to see (Specs/m1-core.md, "Statements", "Proofs", "IDs" and
// "Validation"). Paper stubs join them with #8; check histories with #6.

#include "LoomCore/Problem.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/StatementsAndProofs.h"

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

/// Everything in `Statements/` and `Proofs/`, each list sorted by path.
struct ProjectContents {
    std::vector<StatementFile> statements;
    std::vector<ProofFile> proofs;
    std::vector<VerboseFile> verbose;
    /// The `<proof-id>.checks.toml` files, as project-relative paths. #6 reads them.
    QStringList checksFiles;
    /// Problems found while listing the folders: `stray-file`, and `bad-path` for a file that
    /// resolves outside the project.
    std::vector<Problem> listingProblems;

    /// The statement whose file name gives `tag`, or null.
    const StatementFile* statement(const QString& tag) const;
    /// The proof whose file name gives `id`, or null.
    const ProofFile* proof(const QString& id) const;
    /// The tag that has `alias` among its aliases, or an empty string.
    QString tagWithAlias(const QString& alias) const;
};

/// The result of loadContents().
struct ContentsLoading {
    /// False if a file couldn't be read at all (permissions, a disk error); then `message` says which.
    bool ok = false;
    ProjectContents contents;
    QString message;
};

/// Reads every statement, proof and verbose proof of a project (Specs/m1-core.md, "Statements"
/// and "Proofs"). A missing `Statements/` or `Proofs/` folder is the same as an empty one.
/// Rules for what may sit where: `Statements/` holds only `<tag>.tex`; `Proofs/` holds only
/// folders named by a tag; `Proofs/<tag>/` holds only `<tag>.<letters>.tex`, `.verbose.tex` and
/// `.checks.toml`. Anything else is a `stray-file`, and isn't read.
/// @param root The project folder.
/// @returns The files and what reading each gave. Reads files; writes nothing.
ContentsLoading loadContents(const QString& root);

/// Every problem in the project that statements and proofs show: each file's own problems,
/// the listing's, and those that need several files (`tag-unknown`, `id-syntax` and `ref-alias`
/// in references, `id-duplicate`, `alias-duplicate`, `ref-unknown`, `stated-using-self`,
/// `stated-using-cycle`, `superseded-invalid`, `refuted-unknown`, `refuted-no-status`,
/// `proof-folder-unknown`, `proves-mismatch`, `proof-of-no-status`, `premise-no-status`,
/// `verbose-orphan`). Status, checks, papers and bodies are checked by later issues.
/// @param config   The project's `loom.toml`, for its tags.
/// @param contents The statements and proofs.
/// @returns The problems, sorted with sortProblems(). No side effects.
std::vector<Problem> checkContents(const ProjectConfig& config, const ProjectContents& contents);

/// Reads a statement ID a person typed (Specs/m1-core.md, "IDs": tags are forgiving on the
/// command line, and aliases are resolved wherever a person types an ID).
/// @param typed E.g. `4kx7`, `4KX7` or `thm:energy-decay`.
/// @returns The tag of an existing statement, or nothing. No side effects.
std::optional<QString> resolveStatement(const ProjectContents& contents, const QString& typed);

/// Reads a proof ID a person typed, as resolveStatement() reads the tag part.
/// @param typed E.g. `4kx7.A`.
/// @returns The ID of an existing proof, or nothing. No side effects.
std::optional<QString> resolveProof(const ProjectContents& contents, const QString& typed);

} // namespace loom
