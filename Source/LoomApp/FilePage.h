#pragma once

// One page of the editor (Specs/m2-statements-review.md, "The editor": "One statement is open at a
// time, as pages: the Statement, then one page per proof"): a header with the form, the bars that
// say what happened to the file, the find bar and the text editor. A page shows buffers it doesn't
// own; the Statements page keeps them, so leaving a page loses nothing.

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>
#include <tuple>

class QLabel;
class QToolButton;
class QVBoxLayout;

namespace loom {

class EditBuffer;
class FindBar;
class LatexEditor;
class ProofForm;
class StatementForm;
class StatementsPage;

/// A statement's page, a proof's page (with its verbose version), or a paper stub's read-only page.
class FilePage : public QWidget
{
    Q_OBJECT

public:
    /// @param owner   The Statements page, which owns the buffers and does every save.
    /// @param tag     The statement (or stub) shown.
    /// @param proofId The proof of a proof page; empty for the statement's page.
    FilePage(StatementsPage* owner, const QString& tag, const QString& proofId);

    const QString& tag() const { return m_tag; }
    const QString& proofId() const { return m_proofId; }

    /// Shows these buffers: the statement's or proof's file, and for a proof its verbose version
    /// if there is one (or null).
    void bind(EditBuffer* main, EditBuffer* verbose);
    EditBuffer* mainBuffer() const { return m_main; }
    EditBuffer* verboseBuffer() const { return m_verbose; }
    /// The buffer the editor shows: the verbose version when the toggle says so, else the main one.
    /// Null when Verbose is on and there is no verbose version yet.
    EditBuffer* shownBuffer() const;

    /// The toggle on a proof page: **Proof** | **Verbose**.
    bool showingVerbose() const { return m_showingVerbose; }
    void setShowingVerbose(bool verbose);

    /// Re-reads the buffers and the latest snapshot into the header, form, bars and editor,
    /// keeping the editor's cursor and scroll when its text is unchanged.
    void refresh();
    /// Shows a short note in a bar (e.g. why raw mode can't be left), until the next refresh() that
    /// clears it.
    void showNote(const QString& note);

    /// Keeps the editor's cursor and scroll in the shown buffer.
    void storeView();
    /// Puts the cursor on an editor line (from 1) and shows it.
    void goToEditorLine(int line);

    LatexEditor* editor() const { return m_editor; }
    FindBar* findBar() const { return m_findBar; }
    StatementForm* statementForm() const { return m_statementForm; }
    ProofForm* proofForm() const { return m_proofForm; }
    QToolButton* rawToggle() const { return m_raw; }
    QToolButton* proofToggle() const { return m_showProof; }
    QToolButton* verboseToggle() const { return m_showVerbose; }
    /// The bar with this object name (`recoveryBar`, `outsideBar`, `refusalBar`, `staleBar`,
    /// `noteBar`, `stubBar`), or null.
    QWidget* bar(const QString& name) const;

private:
    /// Rebuilds the bars from the buffer's state.
    void refreshBars();
    /// Adds a bar: its text and buttons (object name, label, action).
    QWidget* addBar(const QString& name, const QString& text,
                    const QList<std::tuple<QString, QString, std::function<void()>>>& buttons);
    /// Puts the shown buffer's text into the editor, if it differs.
    void loadEditor();
    /// Marks the lines with problems: the snapshot's for this file, and a refused save's.
    void markProblems();

    StatementsPage* m_owner;
    QString m_tag;
    QString m_proofId;
    EditBuffer* m_main = nullptr;
    EditBuffer* m_verbose = nullptr;
    EditBuffer* m_editorBuffer = nullptr; ///< The buffer the editor's text came from.
    bool m_showingVerbose = false;
    bool m_updating = false;
    QString m_note;

    QLabel* m_heading;
    QLabel* m_status;
    QToolButton* m_raw;
    QToolButton* m_showProof;
    QToolButton* m_showVerbose;
    QWidget* m_bars;
    QVBoxLayout* m_barsLayout;
    StatementForm* m_statementForm = nullptr;
    ProofForm* m_proofForm = nullptr;
    FindBar* m_findBar;
    LatexEditor* m_editor;
    QWidget* m_writeVerbose;
};

} // namespace loom
