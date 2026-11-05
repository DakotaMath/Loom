#include "LoomApp/FindBar.h"

#include "LoomApp/LatexEditor.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace loom {

FindBar::FindBar(QWidget* parent)
    : QWidget(parent)
    , m_find(new QLineEdit(this))
    , m_replace(new QLineEdit(this))
    , m_matchCase(new QCheckBox(QStringLiteral("Match case"), this))
    , m_wholeWord(new QCheckBox(QStringLiteral("Whole word"), this))
    , m_regex(new QCheckBox(QStringLiteral("Regular expression"), this))
    , m_replaceOne(new QPushButton(QStringLiteral("Replace"), this))
    , m_replaceAll(new QPushButton(QStringLiteral("Replace all"), this))
    , m_replaceRow(new QWidget(this))
    , m_note(new QLabel(this))
{
    setObjectName(QStringLiteral("findBar"));
    m_find->setObjectName(QStringLiteral("findText"));
    m_find->setPlaceholderText(QStringLiteral("Find"));
    m_find->setClearButtonEnabled(true);
    m_replace->setObjectName(QStringLiteral("replaceText"));
    m_replace->setPlaceholderText(QStringLiteral("Replace with"));
    m_matchCase->setObjectName(QStringLiteral("matchCase"));
    m_wholeWord->setObjectName(QStringLiteral("wholeWord"));
    m_regex->setObjectName(QStringLiteral("regularExpression"));
    m_replaceOne->setObjectName(QStringLiteral("replaceOne"));
    m_replaceAll->setObjectName(QStringLiteral("replaceAll"));
    m_note->setObjectName(QStringLiteral("findNote"));
    m_note->setForegroundRole(QPalette::PlaceholderText);

    auto* previous = new QToolButton(this);
    previous->setText(QStringLiteral("↑"));
    previous->setToolTip(QStringLiteral("Previous match (Shift+F3)"));
    auto* next = new QToolButton(this);
    next->setText(QStringLiteral("↓"));
    next->setToolTip(QStringLiteral("Next match (F3)"));
    auto* close = new QToolButton(this);
    close->setText(QStringLiteral("×"));
    close->setToolTip(QStringLiteral("Close (Esc)"));
    close->setAutoRaise(true);

    auto* findRow = new QHBoxLayout;
    findRow->addWidget(m_find, 1);
    findRow->addWidget(previous);
    findRow->addWidget(next);
    findRow->addWidget(m_matchCase);
    findRow->addWidget(m_wholeWord);
    findRow->addWidget(m_regex);
    findRow->addWidget(m_note);
    findRow->addWidget(close);
    auto* replaceRow = new QHBoxLayout(m_replaceRow);
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->addWidget(m_replace, 1);
    replaceRow->addWidget(m_replaceOne);
    replaceRow->addWidget(m_replaceAll);
    replaceRow->addStretch(0);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);
    layout->addLayout(findRow);
    layout->addWidget(m_replaceRow);

    const auto options = [this] {
        return LatexEditor::FindOptions{m_matchCase->isChecked(), m_wholeWord->isChecked(), m_regex->isChecked()};
    };
    connect(m_find, &QLineEdit::returnPressed, this, [this] { findNext(true); });
    connect(next, &QToolButton::clicked, this, [this] { findNext(true); });
    connect(previous, &QToolButton::clicked, this, [this] { findNext(false); });
    connect(close, &QToolButton::clicked, this, [this] {
        hide();
        if (m_editor)
            m_editor->setFocus();
    });
    connect(m_replaceOne, &QPushButton::clicked, this, [this, options] {
        if (!m_editor)
            return;
        m_note->setText(m_editor->replaceOne(m_find->text(), m_replace->text(), options()) ? QString() : QStringLiteral("No match selected"));
    });
    connect(m_replaceAll, &QPushButton::clicked, this, [this, options] {
        if (!m_editor)
            return;
        const int n = m_editor->replaceAll(m_find->text(), m_replace->text(), options());
        m_note->setText(n == 1 ? QStringLiteral("1 replaced") : QStringLiteral("%1 replaced").arg(n));
    });
    hide();
}

void FindBar::setEditor(LatexEditor* editor)
{
    m_editor = editor;
    m_replaceOne->setEnabled(editor && !editor->isReadOnly());
    m_replaceAll->setEnabled(editor && !editor->isReadOnly());
}

void FindBar::open(bool replace)
{
    m_replaceRow->setVisible(replace);
    if (m_editor && m_editor->hasSelectedText()) {
        const QString selected = m_editor->selectedText();
        if (!selected.contains(u'\n'))
            m_find->setText(selected);
    }
    m_note->clear();
    show();
    m_find->setFocus();
    m_find->selectAll();
}

bool FindBar::findNext(bool forward)
{
    if (!m_editor || m_find->text().isEmpty())
        return false;
    const bool found = m_editor->find(m_find->text(), {m_matchCase->isChecked(), m_wholeWord->isChecked(), m_regex->isChecked()}, forward);
    m_note->setText(found ? QString() : QStringLiteral("No matches"));
    return found;
}

void FindBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        if (m_editor)
            m_editor->setFocus();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace loom
