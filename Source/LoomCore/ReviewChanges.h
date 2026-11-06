#pragma once

// What Review shows of the uncommitted changes (Specs/m2-statements-review.md, "The Review tab" and
// "Rules: Front-matter changes in plain words"): the changed files grouped into entries, and each
// file's changes in plain words, computed between two contents of the project (`HEAD` and now).
// The app only draws them; nothing here reads the disk or runs git.

#include "LoomCore/GitOperations.h"
#include "LoomCore/StatusChanges.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

struct ProjectContents;

/// One change in plain words.
struct ChangeLine {
    QString text;         ///< `premises: +7RT2 · A tail bound, −9D3F · …`
    /// A word or two for the list's summary of the entry (*"text, 4KX7.a premises, check"*):
    /// `text`, `premises`, `level`, `check`, `new`, `deleted`, `verbose`, or the field's name.
    QString what;
    bool warning = false; ///< A change M1 warns about or refuses (`deleted-since-commit`, `check-changed`).
};

/// The changes of one file of an entry.
struct FileChanges {
    QString path;         ///< Project-relative, with `/`.
    FileChange change = FileChange::Changed;
    /// What the file is, for the heading above its lines: `Statement`, `Proof 4KX7.a`, `Checks of
    /// 4KX7.a`, `Verbose version of 4KX7.a`, `paper.toml`, `Stub 7RT2`, or the path.
    QString label;
    /// For a proof, check history or verbose version: its proof's ID; else empty.
    QString proofId;
    std::vector<ChangeLine> lines;
};

/// Review's groups, in the list's order after *Status changes*.
enum class ReviewGroup { Statements, Papers, Markdown, ProjectFiles, Other };

/// The group's heading: `Statements and proofs`, `Papers`, `Markdown`, `Project files`, `Other files`.
/// No side effects.
QString reviewGroupName(ReviewGroup group);

/// One entry of the list.
struct ReviewEntry {
    ReviewGroup group = ReviewGroup::Other;
    /// The statement's tag (Statements), the paper's folder (Papers), or the path (the rest).
    QString key;
    /// What the list shows: `4KX7 · Energy decay`, `SmithJones24 · A paper's title`, or the path.
    QString name;
    /// New, changed or deleted: for a statement, as its statement file is (changed when only its
    /// proofs are); for a paper, as its `paper.toml` is; else as the file is.
    FileChange change = FileChange::Changed;
    /// For Statements and Papers: what changed in a few words, *"text, 4KX7.a premises, check"*.
    QString summary;
    /// The changed files, sorted by path.
    std::vector<ChangedFile> files;
    /// For Statements and Papers: each file's plain-word changes, statement (or `paper.toml`)
    /// first, then by path. Empty for the other groups, which show only the raw diff.
    std::vector<FileChanges> details;
    /// Whether the statement exists at `HEAD` and now (Statements only), for the two builds.
    bool before = false;
    bool after = false;
};

/// Everything Review lists.
struct ReviewList {
    /// The statements and stubs whose status differs between `HEAD` and now (statusChanges()).
    std::vector<StatusChange> statusChanges;
    /// The entries, by group, then by key.
    std::vector<ReviewEntry> entries;

    /// The badge: every entry but the status changes (the spec: "The Review badge counts the
    /// entries of every group but Status changes"). No side effects.
    int count() const { return static_cast<int>(entries.size()); }
};

/// Which group a changed file goes in (Specs/m2-statements-review.md, "The Review tab", the
/// table of groups). Rules, first match wins: `Statements/<tag>.tex` and anything in
/// `Proofs/<tag>/` are Statements (key: the tag); `Papers/<Key>/paper.toml` and
/// `Papers/<Key>/Statements/*.toml` are Papers (key: the folder); `loom.toml`, anything in `TeX/`,
/// `CLAUDE.md`, `.gitignore` and `.gitattributes` are Project files; any other `.md` file is
/// Markdown; anything else is Other.
/// @param path Project-relative, with `/`.
/// @param key  Set to the entry's key.
/// @returns The group. No side effects.
ReviewGroup reviewGroupOf(const QString& path, QString& key);

/// The changes of one file in plain words, between its version in `before` and in `after`
/// (Specs/m2-statements-review.md, "Rules: Front-matter changes in plain words"): one line per
/// change, in the field order of M1's tables, IDs with their titles.
/// Rules:
/// - A new statement: `new theorem 4KX7 · Energy decay`; a new proof: `new proof 4KX7.b (sketch,
///   from 5HN8 · …)`; a new check history lists its checks as added; a new stub: `new lemma 7RT2 ·
///   …`; a new verbose version: `verbose version added`.
/// - A deleted statement, proof or stub: `deleted`, with M1's `deleted-since-commit` warning as a
///   second line; a deleted verbose version: `verbose version removed`; any other file: `deleted`.
/// - Fields: `title: "Old" → "New"`; `kind: lemma → theorem`; `origin: new → …`; lists as
///   `tags: +a, −b` (`stated using`, `premises`, `paper premises` with titles); `level: sketch →
///   complete`; `superseded by 4KX7 · …` / `no longer superseded`; `refuted by G5TZ.a (accepted
///   2026-11-06)` / `no longer refuted`; `front matter verified 2026-11-05` / `front matter no
///   longer verified`; other fields as `name: old → new`.
/// - The body: `text changed`; when the change of this file alone outdates checks (outdatedChecks()
///   of `before` with only this file changed), `text changed; outdates the checks of 4KX7.a, P3QA.a`.
/// - Checks: committed checks are matched in order to the current ones, as M1's `check-changed`
///   matches them; an unmatched committed check is `check changed: Dakota, 2026-11-04 10:00`
///   (a warning); every current check left over is added: `check: Dakota pass`, `check: agent
///   claude-opus-5-5 needs-changes`, `check: Dakota needs-changes: "Step 3 uses …"` (the first
///   line of `changes`), each with `(this version)` when its fingerprint is the proof's now, else
///   `(an older version)`.
/// - A verbose version: `verbose version changed` when its body changed, else `verbose version
///   marked up to date` when its fingerprint did.
/// - `paper.toml`: each top-level field, `[work]` field, version and version field, reading and
///   renumbering that changed: `journal: "…" → "…"` (a version's field prefixed by its ID when the
///   paper has several), `title verified 2026-11-05` / `title no longer verified`, `version
///   arxiv-v2 added` / `removed`, `reading added: read arxiv-v1, 2026-11-05`.
/// - A stub: its fields as for a statement, `gist` as text, and its locators: `locator added:
///   arxiv-v1 at "Theorem 2.1"`, `locator removed: …`, `arxiv-v1 at: "…" → "…"`, `arxiv-v1
///   checked against the PDF 2026-11-05` / `… no longer checked against the PDF`.
/// - A file Loom doesn't read (or can't parse on either side): `changed`, `new` or `deleted`.
/// @param path   Project-relative.
/// @param change How git sees the file.
/// @returns The lines; empty if nothing Loom reads differs (a comment changed). No side effects.
FileChanges fileChanges(const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change);

/// Review's list (Specs/m2-statements-review.md, "The Review tab"): the status changes between
/// `before` and `after`, and one entry per statement, paper or other file with a change, each
/// with its files and plain-word changes.
/// @param files  The changed files, as parseGitStatus() reads them.
/// @param before The project at `HEAD` (empty if there is no commit yet).
/// @param after  The project now.
/// @returns The list. No side effects.
ReviewList reviewList(const std::vector<ChangedFile>& files, const ProjectContents& before, const ProjectContents& after);

} // namespace loom
