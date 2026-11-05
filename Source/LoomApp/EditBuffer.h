#pragma once

// One file open in the Statements tab, with its unsaved edits (Specs/m2-statements-review.md,
// "The editor", "Saving" and "Never losing text"): what was loaded from disk (its `base`), the
// form's fields and the body, or the raw text, and what has happened to the file outside Loom.
// No widgets: the pages show a buffer, and the buffer outlives them, so edits survive leaving the
// statement or the tab.

#include "LoomCore/Problem.h"
#include "LoomCore/StatementsAndProofs.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <vector>

namespace loom {

/// What kind of file a buffer holds.
enum class FileKind {
    Statement, ///< `Statements/<tag>.tex`: the form (or raw) and the body.
    Proof,     ///< `Proofs/<tag>/<proof-id>.tex`: the form (or raw) and the body.
    Verbose,   ///< `<proof-id>.verbose.tex`: the body only; its front matter is Loom's to keep.
    Stub,      ///< A paper stub, shown read-only (editing papers is M6).
};

/// What happened to the file outside Loom while it was open.
enum class Outside {
    None,
    Changed, ///< It differs from what was loaded, and there are unsaved edits: Dakota chooses.
    Deleted, ///< It is gone.
};

/// A recovery file offered back when the project opened (the spec's table, "On opening a project").
enum class Recovery {
    None,
    Same,    ///< The file is still `base`: *Keep* / *Discard*.
    Changed, ///< The file changed since `base`: *Compare*, *Keep mine*, *Discard mine*.
    Gone,    ///< The file was deleted: *Restore it with my text*, *Discard*.
};

/// Whether two statements have the same fields and body. No side effects.
bool sameStatement(const Statement& a, const Statement& b);
/// Whether two proofs have the same fields and body. No side effects.
bool sameProof(const Proof& a, const Proof& b);

/// A file open for editing.
class EditBuffer
{
public:
    /// A buffer for `path`.
    /// @param path The file, project-relative.
    /// @param kind What it is.
    /// @param id   The statement's tag, or the proof's ID (for a verbose version too), or the stub's tag.
    EditBuffer(QString path, FileKind kind, QString id);

    const QString& path() const { return m_path; }
    FileKind kind() const { return m_kind; }
    const QString& id() const { return m_id; }
    bool readOnly() const { return m_kind == FileKind::Stub; }

    /// Loads the file as it is on disk, dropping any unsaved edits. A statement or proof whose
    /// front matter Loom can't fully read opens in raw mode (Specs/m2-statements-review.md, "Raw
    /// mode is how a file whose front matter doesn't parse is opened and fixed"); otherwise the
    /// mode it was in stays.
    /// @param bytes  The file's bytes; ignored if it doesn't exist.
    /// @param exists Whether the file exists.
    void load(const QByteArray& bytes, bool exists);
    /// Makes `bytes` the file the edits are based on, keeping the edits (*Keep mine*, and after a
    /// save, with the bytes the save wrote). Clears `outside`.
    void rebase(const QByteArray& bytes, bool exists);

    /// The bytes the edits are based on, and whether that file exists.
    const QByteArray& loaded() const { return m_loaded; }
    bool exists() const { return m_exists; }
    /// fileBase() of loaded(), or empty if the file doesn't exist (Specs/m2-statements-review.md,
    /// "Saving": `base`).
    QString base() const;

    /// Whether there are unsaved edits: the text that would be saved differs from what was loaded
    /// (in the form: any field or the body; raw: any byte).
    bool dirty() const;
    /// The text a save writes, and a recovery file keeps: for a statement or proof the whole file,
    /// the front matter composed from the form in canonical form (formatStatement(),
    /// formatProof()) or the raw text as typed; for a verbose version its body; for a stub what was
    /// loaded.
    QByteArray text() const;
    /// Puts a recovered or typed whole text into the buffer: a statement's or proof's whole file
    /// (read into the form if Loom can read every field, else raw), a verbose version's body.
    void setText(const QByteArray& text);

    /// Raw mode: the whole file in the text editor (statements and proofs only).
    bool raw() const { return m_raw; }
    /// Switches between the form and raw text, carrying the edits across.
    /// @param why Set, when the switch can't happen, to why: leaving raw mode needs front matter
    ///            Loom can read in full.
    /// @returns Whether it switched (or was already so).
    bool setRaw(bool raw, QString* why = nullptr);

    /// The form's fields (statements); the body is body().
    const Statement& statement() const { return m_statement; }
    void setStatement(const Statement& statement);
    /// The form's fields (proofs); the body is body().
    const Proof& proof() const { return m_proof; }
    void setProof(const Proof& proof);
    /// The document as loaded, for comparing and for read-only lines (aliases, created, …).
    const Statement& loadedStatement() const { return m_loadedStatement; }
    const Proof& loadedProof() const { return m_loadedProof; }
    const VerboseProof& loadedVerbose() const { return m_loadedVerbose; }
    /// The file line the body starts on in the loaded file (for mapping problems in the form), or 0.
    int loadedBodyLine() const { return m_loadedBodyLine; }

    /// What the text editor shows: the body in the form, the whole file raw.
    QString editorText() const;
    /// Takes the text editor's text: the body in the form, the whole file raw.
    void setEditorText(const QString& text);

    /// What happened to the file outside Loom, and the bytes on disk now (for Changed).
    Outside outside() const { return m_outside; }
    const QByteArray& outsideBytes() const { return m_outsideBytes; }
    void setOutside(Outside outside, const QByteArray& bytes = {});

    /// The recovery bar to show, and when the recovered text was written.
    Recovery recovery() const { return m_recovery; }
    const QDateTime& recoveredAt() const { return m_recoveredAt; }
    void setRecovery(Recovery recovery, const QDateTime& when = {});

    /// The text last written to this file's recovery file; empty if none is kept.
    const QByteArray& drafted() const { return m_drafted; }
    void setDrafted(const QByteArray& text) { m_drafted = text; }

    /// The last refused save's problems and message, shown until the next save.
    const std::vector<Problem>& refusalProblems() const { return m_refusalProblems; }
    const QString& refusalMessage() const { return m_refusalMessage; }
    void setRefusal(const QString& message, const std::vector<Problem>& problems);

    /// Where the editor was, kept while another file is shown.
    int cursorLine = 0;
    int cursorIndex = 0;
    int scroll = 0;

private:
    /// Reads the loaded bytes into the loaded documents.
    void readLoaded();

    QString m_path;
    FileKind m_kind;
    QString m_id;
    QByteArray m_loaded;
    bool m_exists = false;
    bool m_raw = false;
    Statement m_loadedStatement;
    Proof m_loadedProof;
    VerboseProof m_loadedVerbose;
    bool m_loadedLossless = false;
    int m_loadedBodyLine = 0;
    Statement m_statement;
    Proof m_proof;
    QString m_body;
    QString m_rawText;
    Outside m_outside = Outside::None;
    QByteArray m_outsideBytes;
    Recovery m_recovery = Recovery::None;
    QDateTime m_recoveredAt;
    QByteArray m_drafted;
    std::vector<Problem> m_refusalProblems;
    QString m_refusalMessage;
};

} // namespace loom
