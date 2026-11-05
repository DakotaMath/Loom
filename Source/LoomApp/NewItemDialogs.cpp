#include "LoomApp/NewItemDialogs.h"

#include "LoomApp/HeaderForms.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/ProjectSnapshot.h"
#include "LoomApp/StatusMarks.h"

#include "LoomCore/StatementsAndProofs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolButton>
#include <QVBoxLayout>

namespace loom {

namespace {

/// The refusal line under a form: red text, hidden while empty.
QLabel* refusalLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName(QStringLiteral("refusal"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, QColor(0xc62828));
    label->setPalette(palette);
    label->hide();
    return label;
}

/// Asks before a form with text is closed, since closing it discards the text and its recovery
/// file ("Never losing text"). Cancel is focused, as in every confirmation.
/// @param discard Called if Dakota chooses **Discard**. Side effects: opens a window-modal box.
void askToDiscard(QDialog* dialog, const QString& what, std::function<void()> discard)
{
    auto* box = new QMessageBox(QMessageBox::Question, QStringLiteral("Discard %1").arg(what),
                                QStringLiteral("Discard %1? It hasn't been created, and its text goes.").arg(what),
                                QMessageBox::NoButton, dialog);
    box->setObjectName(QStringLiteral("discardNewForm"));
    box->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* yes = box->addButton(QStringLiteral("Discard"), QMessageBox::DestructiveRole);
    yes->setObjectName(QStringLiteral("confirm"));
    QPushButton* cancel = box->addButton(QMessageBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    box->setDefaultButton(cancel);
    box->setEscapeButton(cancel);
    QObject::connect(box, &QMessageBox::finished, dialog, [box, yes, discard] {
        if (box->clickedButton() == yes)
            discard();
    });
    box->open();
    cancel->setFocus();
}

} // namespace

QStringList idsFromField(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[,\\s]+"));
    return text.split(separators, Qt::SkipEmptyParts);
}

NewStatementDialog::NewStatementDialog(const ProjectSnapshot* snapshot, QWidget* parent)
    : QDialog(parent)
    , m_snapshot(snapshot)
    , m_kind(new QComboBox(this))
    , m_title(new QLineEdit(this))
    , m_tags(new ChipList(QStringLiteral("tags"), this))
    , m_statedUsing(new QLineEdit(this))
    , m_body(new LatexEditor(this))
    , m_refusal(refusalLabel(this))
{
    setObjectName(QStringLiteral("newStatementDialog"));
    setWindowTitle(QStringLiteral("New statement"));
    resize(720, 560);
    m_kind->setObjectName(QStringLiteral("kind"));
    for (const QString& kind : statementKinds)
        m_kind->addItem(kindTitle(kind), kind);
    m_kind->setCurrentIndex(m_kind->findData(QStringLiteral("lemma")));
    m_title->setObjectName(QStringLiteral("title"));
    m_statedUsing->setObjectName(QStringLiteral("statedUsing"));
    m_statedUsing->setPlaceholderText(QStringLiteral("Tags of the definitions it uses, separated by commas"));
    m_body->setObjectName(QStringLiteral("body"));

    auto* buttons = new QDialogButtonBox(this);
    m_create = buttons->addButton(QStringLiteral("Create"), QDialogButtonBox::AcceptRole);
    m_create->setObjectName(QStringLiteral("create"));
    QPushButton* cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Kind"), m_kind);
    form->addRow(QStringLiteral("Title"), m_title);
    form->addRow(QStringLiteral("Tags"), m_tags);
    form->addRow(QStringLiteral("Stated using"), m_statedUsing);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("The statement (LaTeX):"), this));
    layout->addWidget(m_body, 1);
    layout->addWidget(m_refusal);
    layout->addWidget(buttons);

    connect(m_create, &QPushButton::clicked, this, &NewStatementDialog::createClicked);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &NewStatementDialog::changed);
    connect(m_title, &QLineEdit::textEdited, this, &NewStatementDialog::changed);
    connect(m_statedUsing, &QLineEdit::textEdited, this, &NewStatementDialog::changed);
    connect(m_body, &LatexEditor::textChanged, this, &NewStatementDialog::changed);
    const auto showTags = [this] { m_tags->setItems(m_tagItems, [](const QString& t) { return std::pair<QString, QIcon>{t, QIcon()}; }); };
    connect(m_tags, &ChipList::addClicked, this, [this, showTags] {
        QMenu menu;
        if (m_snapshot) {
            for (const ProjectTag& tag : m_snapshot->config.tags) {
                if (!m_tagItems.contains(tag.id))
                    menu.addAction(QStringLiteral("%1 · %2").arg(tag.id, tag.title))->setData(tag.id);
            }
        }
        if (menu.isEmpty())
            menu.addAction(QStringLiteral("No more tags in loom.toml"))->setEnabled(false);
        if (QAction* chosen = menu.exec(m_tags->addButton()->mapToGlobal(QPoint(0, m_tags->addButton()->height())));
            chosen && chosen->data().isValid()) {
            m_tagItems << chosen->data().toString();
            showTags();
            emit changed();
        }
    });
    connect(m_tags, &ChipList::removeClicked, this, [this, showTags](const QString& tag) {
        m_tagItems.removeAll(tag);
        showTags();
        emit changed();
    });
}

NewStatement NewStatementDialog::request() const
{
    NewStatement request;
    request.kind = m_kind->currentData().toString();
    request.title = m_title->text().trimmed();
    request.tags = m_tagItems;
    request.statedUsing = idsFromField(m_statedUsing->text());
    request.body = m_body->text().toUtf8();
    return request;
}

QByteArray NewStatementDialog::draftText() const
{
    const NewStatement r = request();
    Statement s;
    s.kind = r.kind;
    s.title = m_title->text();
    s.tags = r.tags;
    s.statedUsing = r.statedUsing;
    s.body = r.body;
    return formatStatement(s);
}

void NewStatementDialog::fill(const QByteArray& text)
{
    const Statement s = readStatement(text, QStringLiteral("Statements/new.tex")).document;
    if (const int index = m_kind->findData(s.kind); index >= 0)
        m_kind->setCurrentIndex(index);
    m_title->setText(s.title);
    m_tagItems = s.tags;
    m_tags->setItems(m_tagItems, [](const QString& t) { return std::pair<QString, QIcon>{t, QIcon()}; });
    m_statedUsing->setText(s.statedUsing.join(QStringLiteral(", ")));
    m_body->setText(QString::fromUtf8(s.body));
}

void NewStatementDialog::showRefusal(const QString& message)
{
    m_refusal->setText(message);
    m_refusal->setVisible(!message.isEmpty());
}

void NewStatementDialog::setBusy(bool busy)
{
    m_create->setEnabled(!busy);
}

void NewStatementDialog::reject()
{
    if (m_discarding || !hasText()) {
        QDialog::reject();
        return;
    }
    askToDiscard(this, QStringLiteral("the new statement"), [this] {
        m_discarding = true;
        QDialog::reject();
    });
}

bool NewStatementDialog::hasText() const
{
    return !m_title->text().trimmed().isEmpty() || !m_body->text().trimmed().isEmpty() || !m_statedUsing->text().trimmed().isEmpty();
}

NewProofDialog::NewProofDialog(const QString& tag, QWidget* parent)
    : QDialog(parent)
    , m_tag(tag)
    , m_level(new QComboBox(this))
    , m_premises(new QLineEdit(this))
    , m_body(new LatexEditor(this))
    , m_refusal(refusalLabel(this))
{
    setObjectName(QStringLiteral("newProofDialog"));
    setWindowTitle(QStringLiteral("New proof of %1").arg(tag));
    resize(720, 520);
    m_level->setObjectName(QStringLiteral("level"));
    // Not `failed`: a dead end is recorded by marking a proof failed later (the spec).
    for (const QString& level : proofLevels) {
        if (level != u"failed")
            m_level->addItem(level, level);
    }
    m_level->setCurrentIndex(m_level->findData(QStringLiteral("complete")));
    m_premises->setObjectName(QStringLiteral("premises"));
    m_premises->setPlaceholderText(QStringLiteral("Tags of the statements it uses, separated by commas"));
    m_body->setObjectName(QStringLiteral("body"));

    auto* buttons = new QDialogButtonBox(this);
    m_create = buttons->addButton(QStringLiteral("Create"), QDialogButtonBox::AcceptRole);
    m_create->setObjectName(QStringLiteral("create"));
    QPushButton* cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Level"), m_level);
    form->addRow(QStringLiteral("Premises"), m_premises);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("The proof (LaTeX):"), this));
    layout->addWidget(m_body, 1);
    layout->addWidget(m_refusal);
    layout->addWidget(buttons);

    connect(m_create, &QPushButton::clicked, this, &NewProofDialog::createClicked);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_level, &QComboBox::currentIndexChanged, this, &NewProofDialog::changed);
    connect(m_premises, &QLineEdit::textEdited, this, &NewProofDialog::changed);
    connect(m_body, &LatexEditor::textChanged, this, &NewProofDialog::changed);
}

NewProof NewProofDialog::request() const
{
    NewProof request;
    request.statement = m_tag;
    request.level = m_level->currentData().toString();
    request.premises = idsFromField(m_premises->text());
    request.body = m_body->text().toUtf8();
    return request;
}

QByteArray NewProofDialog::draftText() const
{
    const NewProof r = request();
    Proof p;
    p.proves = m_tag;
    p.level = r.level;
    p.premises = r.premises;
    p.body = r.body;
    return formatProof(p);
}

void NewProofDialog::fill(const QByteArray& text)
{
    const Proof p = readProof(text, QStringLiteral("Proofs/%1/%1.new.tex").arg(m_tag)).document;
    if (const int index = m_level->findData(p.level); index >= 0)
        m_level->setCurrentIndex(index);
    m_premises->setText(p.premises.join(QStringLiteral(", ")));
    m_body->setText(QString::fromUtf8(p.body));
}

void NewProofDialog::showRefusal(const QString& message)
{
    m_refusal->setText(message);
    m_refusal->setVisible(!message.isEmpty());
}

void NewProofDialog::setBusy(bool busy)
{
    m_create->setEnabled(!busy);
}

void NewProofDialog::reject()
{
    if (m_discarding || !hasText()) {
        QDialog::reject();
        return;
    }
    askToDiscard(this, QStringLiteral("the new proof"), [this] {
        m_discarding = true;
        QDialog::reject();
    });
}

bool NewProofDialog::hasText() const
{
    return !m_body->text().trimmed().isEmpty() || !m_premises->text().trimmed().isEmpty();
}

} // namespace loom
