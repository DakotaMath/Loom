#pragma once

// Finding statements, proofs, paper results and papers by what a person remembers of them, and
// reading the IDs a LaTeX body mentions (Specs/m2-statements-review.md, "IDs in the editor"): the
// ID picker's search, the Statements list's search, the titles shown beside IDs, and what a hover
// or Ctrl+click on an ID opens. Nothing here touches a widget or the disk.

#include "LoomCore/ProjectContents.h"
#include "LoomCore/Status.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <optional>
#include <vector>

namespace loom {

/// What an ID names.
enum class IdKind {
    Statement, ///< A statement of `Statements/`.
    Proof,     ///< A proof, `<tag>.<letters>`.
    Stub,      ///< A paper stub (a paper's result).
    Paper,     ///< A paper entry, by its key.
    Label,     ///< A `\label` in some body: `\ref` may name it, but it is nothing Loom keeps.
};

/// Where an ID is being chosen; each offers different things (Specs/m2-statements-review.md, "IDs
/// in the editor", "What it offers").
enum class IdField {
    Ref,          ///< `\ref` and *Go to statement*: statements, proofs and stubs.
    PaperRef,     ///< `\paperref`: stubs only.
    Premises,     ///< A proof's premises: provable statements and stubs, not the proof's own statement.
    StatedUsing,  ///< A statement's `stated_using` (and the list's search): statements and stubs.
    SupersededBy, ///< A statement's `superseded_by`: statements.
    Cite,         ///< `\cite`: paper keys.
    /// **Refute…** (Specs/m2-statements-review.md, "Status and checks in the tab"): proofs of
    /// other statements than `self`, the statement being refuted.
    RefutedBy,
};

/// One thing the search can find, with the words it is found by (folded: lower case, no accents).
struct IdEntry {
    QString id;        ///< A statement's or stub's tag, a proof ID, or a paper's current key.
    IdKind kind = IdKind::Statement;
    /// For a statement or stub, its kind (`theorem`, …); for a proof, its level; for a paper, `paper`.
    QString kindName;
    /// What `ID · title` shows after the ID: a statement's title; a proof's `proof of {statement's
    /// title}`; a stub's `{Key} {locator} · {title or gist}`; a paper's `{first author} · {title}`.
    QString title;
    /// For a proof, the tag of its statement; else empty.
    QString statement;
    /// Whether it may be a premise: a statement or stub of a provable kind (isProvableKind()).
    bool provable = false;

    QStringList names;      ///< Folded: the ID and every alias (for a paper, its folder and old keys).
    QStringList nameWords;  ///< Folded: the names and their parts (`thm:energy-decay` → `thm`, `energy`, `decay`).
    QStringList titleWords; ///< The words of the title (a stub's: its paper's key, locator and title; a paper's: its authors).
    QStringList tagWords;   ///< The project tags it has (`tail-bounds`, `tail`, `bounds`).
    QStringList bodyWords;  ///< The words of its body (a stub's gist, a paper's summary).

    /// `ID · title`, as the picker, the list and the annotations show it. No side effects.
    QString label() const;
};

/// One search result.
struct IdMatch {
    const IdEntry* entry = nullptr; ///< Points into the IdIndex searched; valid while it lives.
    /// How it matched, best first: 0 the query is exactly its ID or an alias; 1 every word is found
    /// in its ID, aliases or title; 2 some word needs its tags; 3 some word needs its body.
    int tier = 0;
};

/// What `\ref`, `\paperref` or `\cite` with a name points to.
struct IdResolution {
    bool found = false;  ///< False: it names nothing (the editor underlines it in red).
    IdKind kind = IdKind::Statement;
    /// The entry it names (its tag for an alias, the current key for an old or wrong-case key);
    /// for a Label, the label itself.
    QString id;
    bool alias = false;  ///< It was named by an alias or old key, not by its ID.
};

/// An ID mentioned in LaTeX text: one name in the braces of `\ref`, `\paperref` or `\cite`
/// (each key of `\cite{A,B}` separately).
struct IdMention {
    QString command;      ///< `ref`, `paperref` or `cite`.
    QString name;         ///< As written, trimmed.
    int line = 1;         ///< The text's line, from 1.
    qsizetype start = 0;  ///< Where the name starts in its line, in UTF-16 code units.
    qsizetype length = 0; ///< Its length, in UTF-16 code units.
};

/// An ID being typed: the cursor is inside the braces of `\ref`, `\paperref` or `\cite`.
struct IdTyping {
    QString command;      ///< `ref`, `paperref` or `cite`.
    qsizetype start = 0;  ///< Where the name being typed starts in the line (after `{`, or `,` in `\cite`).
    QString typed;        ///< What is typed of it so far, up to the cursor.
};

/// Folds text for matching: lower case, accents removed (é → e, ö → o). No side effects.
QString foldForSearch(const QString& text);

/// The folded words of a text: runs of letters and digits. No side effects.
QStringList searchWords(const QString& text);

/// Whether every word of `query` starts a word of `words` (both folded or not: the query is
/// folded here). An empty query matches. No side effects.
bool matchesEveryWord(const QString& query, const QStringList& words);

/// Every statement, proof, paper stub and paper of a project, indexed for the ID picker and for
/// reading IDs in the editor. Built once per snapshot of the project; immutable after.
class IdIndex
{
public:
    IdIndex() = default;
    /// Indexes a project. @param contents Its files. @param graph Its graph (for statement order).
    /// No side effects; keeps copies of what it needs, not references.
    IdIndex(const ProjectContents& contents, const ProjectGraph& graph);

    /// Searches (Specs/m2-statements-review.md, "IDs in the editor", "What it searches").
    /// Rules: the query is split into words, and a result must have every word as the start of
    /// one of its words (ignoring case and accents), from its ID, aliases, title, tags or body. The
    /// whole query equal to its ID or an alias (as a typed tag is read: `4kx7`, `O`→`0`) ranks
    /// first, then results found by ID, alias and title alone, then those that need their tags,
    /// then those that need their body. Ties go to the most recently opened (`recent`, most recent
    /// first), then to the ID in tag order. An empty query finds everything the field offers.
    /// @param query  What is typed.
    /// @param field  What the field offers (IdField).
    /// @param self   The statement being edited, which premises, stated using and superseded by
    ///               never offer; empty for none.
    /// @param recent Tags and proof IDs most recently opened first.
    /// @returns The matches, best first. No side effects.
    std::vector<IdMatch> search(const QString& query, IdField field, const QString& self = {},
                                const QStringList& recent = {}) const;

    /// The entry with this exact ID (a tag, proof ID or current key), or null. No side effects.
    const IdEntry* entry(const QString& id) const;

    /// What a name in `\ref`, `\paperref` or `\cite` points to (Specs/m1-core.md, "IDs and
    /// references", and Specs/m2-statements-review.md, "`\paperref` and `\cite`").
    /// Rules: `\ref` names a statement or stub by tag or alias, a proof by ID (builds label every
    /// proof), or a `\label` of some body; `\paperref` a stub by tag or alias; `\cite` a paper by
    /// its key or an old key, in any case (builds rewrite it, E3). Tags are exact here, as LaTeX's
    /// labels are.
    /// @returns The resolution; `found` is false if it names nothing. No side effects.
    IdResolution resolve(const QString& command, const QString& name) const;

    /// What an ID typed in a field or chosen means: a tag or alias of a statement or stub (as a
    /// typed tag is read), a proof ID, or a paper's key.
    /// @returns The entry, or null. No side effects.
    const IdEntry* lookup(const QString& typed) const;

    /// Every entry, statements and stubs in tag order, each statement followed by its proofs,
    /// then papers by key.
    const std::vector<IdEntry>& entries() const { return m_entries; }

private:
    std::vector<IdEntry> m_entries;
    QHash<QString, qsizetype> m_byId;           ///< ID to entry index.
    QHash<QString, QString> m_tagOfAlias;       ///< Alias (exact) to tag.
    QHash<QString, QString> m_keyOfFolded;      ///< Folded key or old key to current key.
    QSet<QString> m_labels;                     ///< Every `\label` in any body.
};

/// The IDs a LaTeX text mentions in `\ref`, `\paperref` and `\cite` (also with `[…]`), in order.
/// Comments are skipped, and a command and its braces must be on one line, as latexCommands()
/// reads them. Empty names (`\cite{A,}`) are left out.
/// @param text The text, with LF line endings.
/// @returns The mentions. No side effects.
std::vector<IdMention> idMentions(const QString& text);

/// Whether the cursor is in the braces of `\ref`, `\paperref` or `\cite`, and what is typed there.
/// @param lineBeforeCursor The cursor's line, up to the cursor.
/// @returns The command and the name being typed, or nothing (also inside a comment). No side effects.
std::optional<IdTyping> idBeingTyped(QStringView lineBeforeCursor);

/// The field a LaTeX command's picker offers: `paperref` → PaperRef, `cite` → Cite, else Ref.
/// No side effects.
IdField fieldOfCommand(const QString& command);

} // namespace loom
