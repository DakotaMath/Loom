#pragma once

// The New statement and New proof forms (Specs/m2-statements-review.md, "Creating statements and
// proofs"). Each collects a request for LoomCore's createStatement() or createProof(); the
// Statements page runs it, keeps the form's text in a recovery file while it's open, and closes it
// once the file is created.

#include "LoomCore/Edits.h"

#include <QByteArray>
#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace loom {

class ChipList;
class LatexEditor;
struct ProjectSnapshot;

/// Turns a field of IDs typed with commas or spaces into the list, each as typed. No side effects.
QStringList idsFromField(const QString& text);

/// The New statement form: kind, title, tags, stated using, and the body.
class NewStatementDialog : public QDialog
{
    Q_OBJECT

public:
    /// The recovery file of an open New statement form (Specs/m2-statements-review.md, "Never
    /// losing text": `.loom/drafts/new/<form>.txt`).
    static QString draftName() { return QStringLiteral("new/statement.txt"); }

    /// @param snapshot The project, for its tags; must outlive the dialog.
    NewStatementDialog(const ProjectSnapshot* snapshot, QWidget* parent = nullptr);

    /// What Create asks LoomCore for.
    NewStatement request() const;
    /// The form as a statement file without a tag (formatStatement()), for its recovery file.
    QByteArray draftText() const;
    /// Fills the form from a recovery file's text.
    void fill(const QByteArray& text);
    /// Shows why LoomCore refused, under the form; the form stays as it is.
    void showRefusal(const QString& message);
    /// Shows that Create is running (or not): the button is disabled meanwhile.
    void setBusy(bool busy);
    /// Whether anything has been typed.
    bool hasText() const;

    QComboBox* kind() const { return m_kind; }
    QLineEdit* title() const { return m_title; }
    ChipList* tags() const { return m_tags; }
    QLineEdit* statedUsing() const { return m_statedUsing; }
    LatexEditor* body() const { return m_body; }
    QPushButton* createButton() const { return m_create; }
    QLabel* refusal() const { return m_refusal; }

signals:
    /// Create was clicked.
    void createClicked();
    /// Anything in the form changed.
    void changed();

private:
    const ProjectSnapshot* m_snapshot;
    QComboBox* m_kind;
    QLineEdit* m_title;
    ChipList* m_tags;
    QStringList m_tagItems;
    QLineEdit* m_statedUsing;
    LatexEditor* m_body;
    QLabel* m_refusal;
    QPushButton* m_create;
};

/// The New proof form: level, premises and the body.
class NewProofDialog : public QDialog
{
    Q_OBJECT

public:
    /// The recovery file of an open New proof form for `tag`.
    static QString draftName(const QString& tag) { return QStringLiteral("new/proof-%1.txt").arg(tag); }

    /// @param tag The statement the proof proves.
    NewProofDialog(const QString& tag, QWidget* parent = nullptr);

    const QString& tag() const { return m_tag; }
    NewProof request() const;
    /// The form as a proof file without an ID (formatProof()), for its recovery file.
    QByteArray draftText() const;
    void fill(const QByteArray& text);
    void showRefusal(const QString& message);
    void setBusy(bool busy);
    bool hasText() const;

    QComboBox* level() const { return m_level; }
    QLineEdit* premises() const { return m_premises; }
    LatexEditor* body() const { return m_body; }
    QPushButton* createButton() const { return m_create; }
    QLabel* refusal() const { return m_refusal; }

signals:
    void createClicked();
    void changed();

private:
    QString m_tag;
    QComboBox* m_level;
    QLineEdit* m_premises;
    LatexEditor* m_body;
    QLabel* m_refusal;
    QPushButton* m_create;
};

} // namespace loom
