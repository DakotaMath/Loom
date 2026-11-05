#include "LoomApp/IdAssist.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/IdTools.h"
#include "LoomApp/LatexEditor.h"

#include <Qsci/qscicommand.h>
#include <Qsci/qscicommandset.h>
#include <Qsci/qscilexer.h>
#include <Qsci/qscistyle.h>

#include <QApplication>
#include <QKeyEvent>
#include <QPalette>
#include <QPointer>

#include <algorithm>
#include <map>

namespace loom {

namespace {

/// The annotations' style, shared by every editor: QScintilla gives out fewer than thirty style
/// numbers in all, so one is taken once and its colours set before each use.
QsciStyle& annotationStyle()
{
    static QsciStyle style(-1, QStringLiteral("Titles beside IDs"), QColor(Qt::gray), QColor(Qt::white), QFont());
    return style;
}

/// The byte length of the start of a line, in Scintilla's UTF-8.
int bytes(const QString& text)
{
    return static_cast<int>(text.toUtf8().size());
}

} // namespace

IdAssist::IdAssist(LatexEditor* editor, IdTools* tools)
    : QObject(editor)
    , m_editor(editor)
    , m_tools(tools)
{
    setObjectName(QStringLiteral("idAssist"));
    m_timer.setSingleShot(true);
    m_timer.setInterval(decorateDelayMs);
    connect(&m_timer, &QTimer::timeout, this, &IdAssist::decorate);
    connect(m_editor, &QsciScintilla::textChanged, &m_timer, qOverload<>(&QTimer::start));
    if (m_tools)
        connect(m_tools, &IdTools::snapshotChanged, this, &IdAssist::decorate);

    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETSTYLE, knownIndicator, QsciScintillaBase::INDIC_HIDDEN);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETHOVERSTYLE, knownIndicator, QsciScintillaBase::INDIC_PLAIN);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETHOVERFORE, knownIndicator, dark ? QColor(0x7cb7ff) : QColor(0x1a4fa0));
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETSTYLE, unknownIndicator, QsciScintillaBase::INDIC_SQUIGGLE);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, unknownIndicator, QColor(0xd03030));
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETSTYLE, flashIndicator, QsciScintillaBase::INDIC_ROUNDBOX);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, flashIndicator, QColor(0xd03030));
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETALPHA, flashIndicator, 140);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICSETUNDER, flashIndicator, true);
    m_editor->setAnnotationDisplay(QsciScintilla::AnnotationIndented);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETMOUSEDWELLTIME, 500);

    // Ctrl+Space is the picker's, not QScintilla's (Wiki/gotchas/qt/qscintilla-takes-bound-shortcuts.md).
    if (QsciCommand* command = m_editor->standardCommands()->boundTo(Qt::Key_Space | static_cast<int>(Qt::ControlModifier)))
        command->setKey(0);
    m_editor->installEventFilter(this);

    connect(m_editor, &QsciScintillaBase::SCN_CHARADDED, this, [this](int added) {
        if (added != '{' || m_editor->isReadOnly())
            return;
        int line = 0;
        int index = 0;
        m_editor->getCursorPosition(&line, &index);
        const std::optional<IdTyping> typing = idBeingTyped(QStringView(m_editor->text(line)).first(index));
        if (typing && typing->typed.isEmpty())
            openPickerAtCursor();
    });
    connect(m_editor, &QsciScintillaBase::SCN_DWELLSTART, this, [this](int position, int x, int y) {
        if (!m_tools || position < 0)
            return;
        const Span* span = spanAt(position);
        if (!span || span->resolution.kind == IdKind::Label)
            return;
        const int below = static_cast<int>(m_editor->SendScintilla(QsciScintillaBase::SCI_TEXTHEIGHT, 0));
        m_tools->showHover(span->resolution.found ? span->resolution.id : span->mention.name, m_editor->viewport(),
                           m_editor->viewport()->mapToGlobal(QPoint(x, y + below)));
    });
    connect(m_editor, &QsciScintillaBase::SCN_DWELLEND, this, [this] {
        if (m_tools)
            m_tools->hideHover();
    });
    connect(m_editor, &QsciScintilla::indicatorClicked, this, [this](int line, int index, Qt::KeyboardModifiers state) {
        if (!(state & Qt::ControlModifier))
            return;
        if (const Span* span = spanAt(m_editor->positionFromLineIndex(line, index)))
            activate(*span);
    });
    QTimer::singleShot(0, this, &IdAssist::decorate);
}

void IdAssist::decorate()
{
    m_timer.stop();
    const int length = m_editor->length();
    for (const int indicator : {knownIndicator, unknownIndicator}) {
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, indicator);
        m_editor->SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, 0, length);
    }
    m_editor->clearAnnotations();
    m_spans.clear();
    const IdIndex* index = m_tools ? m_tools->index() : nullptr;
    if (!index)
        return;

    const QString text = m_editor->text();
    const QStringList lines = text.split(u'\n');
    std::map<int, QStringList> titles; // By editor line, from 0.
    for (const IdMention& mention : idMentions(text)) {
        const QString& line = lines[mention.line - 1];
        Span span;
        span.mention = mention;
        span.resolution = index->resolve(mention.command, mention.name);
        span.position = static_cast<int>(m_editor->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, mention.line - 1))
                        + bytes(line.left(mention.start));
        span.length = bytes(line.mid(mention.start, mention.length));
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, span.resolution.found ? knownIndicator : unknownIndicator);
        m_editor->SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE, span.position, span.length);
        if (span.resolution.found && span.resolution.kind != IdKind::Label) {
            if (const IdEntry* entry = index->entry(span.resolution.id)) {
                QStringList& onLine = titles[mention.line - 1];
                if (!onLine.contains(entry->label()))
                    onLine << entry->label();
            }
        }
        m_spans.push_back(span);
    }

    if (!m_tools->settings() || m_tools->settings()->showTitlesBesideIds()) {
        QsciStyle& style = annotationStyle();
        const QPalette palette = QApplication::palette();
        style.setColor(palette.color(QPalette::PlaceholderText));
        style.setPaper(palette.color(QPalette::Base));
        QFont font = m_editor->lexer() ? m_editor->lexer()->defaultFont() : m_editor->font();
        font.setPointSizeF(std::max(6.0, font.pointSizeF() * 0.9));
        style.setFont(font);
        for (const auto& [line, onLine] : titles)
            m_editor->annotate(line, onLine.join(QStringLiteral("      ")), style);
    }
}

bool IdAssist::openPickerAtCursor()
{
    if (!m_tools || m_editor->isReadOnly())
        return false;
    int line = 0;
    int index = 0;
    m_editor->getCursorPosition(&line, &index);
    const QString lineText = m_editor->text(line);
    const std::optional<IdTyping> typing = idBeingTyped(QStringView(lineText).first(index));
    if (!typing)
        return false;
    const int position = static_cast<int>(m_editor->SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS));
    const int height = static_cast<int>(m_editor->SendScintilla(QsciScintillaBase::SCI_TEXTHEIGHT, line));
    const QPoint global = m_editor->viewport()->mapToGlobal(pointOf(position) + QPoint(0, height));
    QPointer<LatexEditor> editor = m_editor;
    const QString command = typing->command;
    m_tools->pick(fieldOfCommand(command), {}, typing->typed, global, [editor, line, command](const QString& id) {
        if (!editor)
            return;
        // Read the line again: the name typed so far, and the rest of it after the cursor, give
        // way to the ID; the brace is closed unless it already is.
        int cursorLine = 0;
        int cursorIndex = 0;
        editor->getCursorPosition(&cursorLine, &cursorIndex);
        if (cursorLine != line)
            return;
        const QString text = editor->text(line);
        const std::optional<IdTyping> now = idBeingTyped(QStringView(text).first(cursorIndex));
        if (!now)
            return;
        qsizetype end = cursorIndex;
        while (end < text.size() && text[end] != u'}' && text[end] != u'\n' && text[end] != u'\r'
               && !(command == u"cite" && text[end] == u','))
            ++end;
        const bool closed = end < text.size() && text[end] == u'}';
        const bool beforeComma = end < text.size() && text[end] == u',';
        editor->beginUndoAction();
        editor->setSelection(line, static_cast<int>(now->start), line, static_cast<int>(end));
        editor->replaceSelectedText(closed || beforeComma ? id : id + u'}');
        editor->endUndoAction();
        // After the closing brace (or the comma of the next key).
        editor->setCursorPosition(line, static_cast<int>(now->start + id.size() + 1));
        editor->setFocus();
    }, m_editor);
    return true;
}

QPoint IdAssist::pointOf(int position) const
{
    return {static_cast<int>(m_editor->SendScintilla(QsciScintillaBase::SCI_POINTXFROMPOSITION, 0, position)),
            static_cast<int>(m_editor->SendScintilla(QsciScintillaBase::SCI_POINTYFROMPOSITION, 0, position))};
}

void IdAssist::activate(const Span& span)
{
    const IdResolution& r = span.resolution;
    if (r.found && (r.kind == IdKind::Statement || r.kind == IdKind::Proof || r.kind == IdKind::Stub)) {
        // Later, not now: opening rebuilds the pages, and this editor may be one of them.
        QPointer<IdTools> tools = m_tools;
        const QString id = r.id;
        QTimer::singleShot(0, m_tools, [tools, id] {
            if (tools)
                tools->requestOpen(id);
        });
        return;
    }
    if (r.found)
        return; // A paper (the Library is M6's) or a `\label`.
    // "Ctrl+click on an ID that names nothing does nothing but flash the red underline."
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, flashIndicator);
    m_editor->SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE, span.position, span.length);
    QPointer<LatexEditor> editor = m_editor;
    QTimer::singleShot(400, this, [editor] {
        if (!editor)
            return;
        editor->SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, flashIndicator);
        editor->SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, 0, editor->length());
    });
}

bool IdAssist::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Space && key->modifiers() == Qt::ControlModifier) {
            openPickerAtCursor();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

const IdAssist::Span* IdAssist::spanAt(int position) const
{
    for (const Span& span : m_spans) {
        if (position >= span.position && position <= span.position + span.length)
            return &span;
    }
    return nullptr;
}

} // namespace loom
