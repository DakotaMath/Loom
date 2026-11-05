#pragma once

// `loom validate` (Specs/m1-core.md, "Validation"): every check of a whole project, in the working
// folder or, with `--staged`, as git's index holds it, so the pre-commit hook sees exactly what
// would be committed. The checks of single files and of the statements, proofs and papers
// together are checkContents()'s and readProjectToml()'s; this file adds those that need the
// computed statuses, the bodies' LaTeX, the disk, or the last commit.

#include "LoomCore/Checks.h"
#include "LoomCore/Git.h"
#include "LoomCore/Problem.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/Status.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// What to validate.
struct ValidationRequest {
    /// The project folder: the one holding `loom.toml`, at the top of its git repository.
    QString root;
    /// Validate git's index (what the next commit would hold) instead of the working folder.
    bool staged = false;
    /// Which git to run. Its environment is passed on as is: in a pre-commit hook, git's
    /// `GIT_INDEX_FILE` must reach `git ls-files`, since `git commit <paths>` stages into a
    /// temporary index.
    GitTool git;
    /// For the app (Specs/m2-statements-review.md, "Opening a folder": a project that isn't in a
    /// git repository, or whose git isn't found, still opens): if git can't read the last commit,
    /// whatever the reason, skip the checks against it rather than fail, and say why in
    /// ValidationResult::commitSkipped. `loom validate` leaves it off. Ignored with `staged`.
    bool commitOptional = false;
};

/// How validating went.
enum class ValidationOutcome {
    Checked,  ///< The project was checked; `problems` says what was found (exit 0, or 1 with errors).
    CantWork, ///< Exit 3: no project, a newer format, a file Loom can't read, or git failed.
};

/// The result of validateProject().
struct ValidationResult {
    ValidationOutcome outcome = ValidationOutcome::CantWork;
    /// For CantWork, a stable code: `not-a-project`, `unreadable`, `format-too-new`,
    /// `git-missing`, `git-timeout` (git didn't finish in time), `not-a-repository` or `git-failed`.
    QString code;
    /// For CantWork, why, in plain English.
    QString message;
    /// Every problem, sorted with sortProblems(): errors first, then warnings, each in file order.
    std::vector<Problem> problems;
    /// With ValidationRequest::commitOptional: why the checks against the last commit were
    /// skipped (git's failure message), or empty if they ran or there was no commit to compare with.
    QString commitSkipped;

    /// How many problems are errors. No side effects.
    int errors() const;
    /// How many problems are warnings. No side effects.
    int warnings() const;
};

/// Validates a project (Specs/m1-core.md, "Validation"): `loom.toml`'s problems, checkContents()'s,
/// checkStatuses()'s, checkBodies()'s, checkDisk()'s and, when the repository has a commit,
/// checkAgainstCommit()'s against `HEAD`.
/// With `staged`, the index is read with `git ls-files --stage` and `git cat-file --batch` into a
/// temporary folder, which is validated in place of the working folder: the files Loom reads in
/// full (`loom.toml`, `Statements/`, `Proofs/`, `Papers/`), the rest as empty files, since only
/// their existence matters (a preamble file, a check's record, a name at the top).
/// @returns The outcome and problems. Side effects: runs git; with `staged`, writes and removes
///          a temporary folder outside the project. Writes nothing in the project.
ValidationResult validateProject(const ValidationRequest& request);

/// The problems that need computed statuses (Specs/m1-core.md, "Validation"): `refuted-but-proved`
/// (a false statement with a complete, checked proof whose premises are all proved or cited),
/// `conjecture-proved`, `premise-false`, `premise-superseded` and `verbose-stale` (a verbose
/// proof's `fingerprint` isn't its proof's current one), all warnings.
/// @param graph buildGraph(contents).
/// @returns The problems, unsorted. No side effects.
std::vector<Problem> checkStatuses(const ProjectContents& contents, const ProjectGraph& graph);

/// The problems in the LaTeX of statement, proof and verbose proof bodies (Specs/m1-core.md,
/// "IDs and references"): the warnings `ref-alias` (a `\ref` or `\paperref` to an old name),
/// `ref-dangling` (a `\ref` to nothing: not a tag, an alias, or a `\label` in any body; a
/// `\paperref` to nothing: not a stub's tag or alias), `ref-not-listed` (a statement's `\ref` or
/// `\paperref` to a statement or stub its `stated_using` doesn't list; a proof's, to one its
/// `premises` don't list, except the statement it proves and statements without a status) and
/// `cite-alias` (a `\cite` of a paper's old key, or of its key or an old key spelt in another
/// case: the same key by A3, but BibTeX matches keys exactly, so the warning names the
/// spelling to write); and the error `paperref-not-stub`, a
/// `\paperref` to a statement of the project rather than a paper stub (Specs/loom.md:
/// `\paperref{<stub-id>}` cites a paper's result).
/// Rules for reading LaTeX: a `%` not escaped by `\` starts a comment, which is skipped; a
/// command and its `{…}` argument must be on one line; optional `[…]` arguments are skipped;
/// `\cite{A, B}` names two keys. `ref-not-listed` is reported once per file and statement.
/// @returns The problems, unsorted. No side effects.
std::vector<Problem> checkBodies(const ProjectContents& contents);

/// The problems that need to look at the project folder itself: `unknown-top-level` for a name
/// at the top that neither M1 nor a later milestone's design uses, and `check-record-not-found`
/// for a check's `record` that doesn't exist (both warnings).
/// @param root The folder to look in: the project, or the staged copy of it.
/// @returns The problems, unsorted. Reads the folder; writes nothing.
std::vector<Problem> checkDisk(const QString& root, const ProjectContents& contents);

/// The project's files as the last commit holds them, for checkAgainstCommit().
struct CommittedFiles {
    /// Every file's project-relative path.
    QStringList paths;
    /// The content of every check history (`Proofs/<tag>/<proof-id>.checks.toml`), by path.
    QHash<QString, QByteArray> checks;
};

/// The problems that need the last commit (Specs/m1-core.md, "Checks", rule 1, and "IDs",
/// "Permanent"): `check-changed`, an error for each committed check that isn't, unchanged and in
/// the same order relative to the other committed checks (A7), in its file now (new entries may
/// sit between committed ones, as a merge interleaves them by date), and
/// `deleted-since-commit`, a warning for each statement, proof or stub in the commit whose ID
/// has no file now (a stub may move to another paper's folder when the paper is rekeyed).
/// @param committed The commit's files.
/// @returns The problems, unsorted. No side effects.
std::vector<Problem> checkAgainstCommit(const ProjectContents& contents, const CommittedFiles& committed);

} // namespace loom
