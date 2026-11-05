#include "LoomApp/EditBuffer.h"

#include "LoomCore/EditorSaves.h"

#include <utility>

namespace loom {

bool sameStatement(const Statement& a, const Statement& b)
{
    return a.id == b.id && a.kind == b.kind && a.title == b.title && a.tags == b.tags && a.statedUsing == b.statedUsing
           && a.aliases == b.aliases && a.origin == b.origin && a.supersededBy == b.supersededBy && a.refutedBy == b.refutedBy
           && a.refutedOn == b.refutedOn && a.created == b.created && a.verified == b.verified && a.body == b.body;
}

bool sameProof(const Proof& a, const Proof& b)
{
    return a.id == b.id && a.proves == b.proves && a.premises == b.premises && a.level == b.level && a.created == b.created
           && a.verified == b.verified && a.body == b.body;
}

EditBuffer::EditBuffer(QString path, FileKind kind, QString id)
    : m_path(std::move(path))
    , m_kind(kind)
    , m_id(std::move(id))
{
    m_raw = m_kind == FileKind::Stub;
}

void EditBuffer::readLoaded()
{
    m_loadedStatement = {};
    m_loadedProof = {};
    m_loadedVerbose = {};
    m_loadedBodyLine = 0;
    // A file that doesn't exist (yet, or any more) has nothing Loom couldn't read.
    m_loadedLossless = true;
    if (!m_exists)
        return;
    switch (m_kind) {
    case FileKind::Statement: {
        const StatementReading reading = readStatement(m_loaded, m_path);
        m_loadedStatement = reading.document;
        m_loadedLossless = reading.parsed && reading.lossless;
        m_loadedBodyLine = reading.bodyLine;
        break;
    }
    case FileKind::Proof: {
        const ProofReading reading = readProof(m_loaded, m_path);
        m_loadedProof = reading.document;
        m_loadedLossless = reading.parsed && reading.lossless;
        m_loadedBodyLine = reading.bodyLine;
        break;
    }
    case FileKind::Verbose: {
        const VerboseReading reading = readVerboseProof(m_loaded, m_path);
        m_loadedVerbose = reading.document;
        m_loadedLossless = reading.parsed && reading.lossless;
        m_loadedBodyLine = reading.bodyLine;
        break;
    }
    case FileKind::Stub:
        break;
    }
}

void EditBuffer::load(const QByteArray& bytes, bool exists)
{
    m_exists = exists;
    m_loaded = exists ? bytes : QByteArray();
    readLoaded();
    m_rawText = QString::fromUtf8(m_loaded);
    switch (m_kind) {
    case FileKind::Statement:
        m_statement = m_loadedStatement;
        m_body = QString::fromUtf8(m_loadedStatement.body);
        if (!m_loadedLossless)
            m_raw = true;
        break;
    case FileKind::Proof:
        m_proof = m_loadedProof;
        m_body = QString::fromUtf8(m_loadedProof.body);
        if (!m_loadedLossless)
            m_raw = true;
        break;
    case FileKind::Verbose:
        m_body = QString::fromUtf8(m_loadedVerbose.body);
        break;
    case FileKind::Stub:
        m_raw = true;
        break;
    }
    m_outside = Outside::None;
    m_outsideBytes.clear();
    m_recovery = Recovery::None;
    m_refusalProblems.clear();
    m_refusalMessage.clear();
}

void EditBuffer::rebase(const QByteArray& bytes, bool exists)
{
    m_exists = exists;
    m_loaded = exists ? bytes : QByteArray();
    readLoaded();
    m_outside = Outside::None;
    m_outsideBytes.clear();
}

QString EditBuffer::base() const
{
    return m_exists ? fileBase(m_loaded) : QString();
}

bool EditBuffer::dirty() const
{
    switch (m_kind) {
    case FileKind::Stub:
        return false;
    case FileKind::Verbose:
        return m_exists ? m_body.toUtf8() != m_loadedVerbose.body : !m_body.isEmpty();
    case FileKind::Statement:
    case FileKind::Proof:
        break;
    }
    if (!m_exists)
        return !editorText().isEmpty() || !m_raw;
    if (m_raw || !m_loadedLossless)
        return text() != m_loaded;
    if (m_kind == FileKind::Statement) {
        Statement now = m_statement;
        now.body = m_body.toUtf8();
        return !sameStatement(now, m_loadedStatement);
    }
    Proof now = m_proof;
    now.body = m_body.toUtf8();
    return !sameProof(now, m_loadedProof);
}

QByteArray EditBuffer::text() const
{
    switch (m_kind) {
    case FileKind::Stub:
        return m_loaded;
    case FileKind::Verbose:
        return m_body.toUtf8();
    case FileKind::Statement:
        if (m_raw)
            return m_rawText.toUtf8();
        {
            Statement s = m_statement;
            s.body = m_body.toUtf8();
            return formatStatement(s);
        }
    case FileKind::Proof:
        if (m_raw)
            return m_rawText.toUtf8();
        {
            Proof p = m_proof;
            p.body = m_body.toUtf8();
            return formatProof(p);
        }
    }
    return {};
}

void EditBuffer::setText(const QByteArray& text)
{
    switch (m_kind) {
    case FileKind::Stub:
        return;
    case FileKind::Verbose:
        m_body = QString::fromUtf8(text);
        return;
    case FileKind::Statement: {
        const StatementReading reading = readStatement(text, m_path);
        m_rawText = QString::fromUtf8(text);
        m_raw = !(reading.parsed && reading.lossless);
        if (!m_raw) {
            m_statement = reading.document;
            m_body = QString::fromUtf8(reading.document.body);
        }
        return;
    }
    case FileKind::Proof: {
        const ProofReading reading = readProof(text, m_path);
        m_rawText = QString::fromUtf8(text);
        m_raw = !(reading.parsed && reading.lossless);
        if (!m_raw) {
            m_proof = reading.document;
            m_body = QString::fromUtf8(reading.document.body);
        }
        return;
    }
    }
}

bool EditBuffer::setRaw(bool raw, QString* why)
{
    if (raw == m_raw)
        return true;
    if (m_kind != FileKind::Statement && m_kind != FileKind::Proof) {
        if (why)
            *why = QStringLiteral("Only statements and proofs have a raw view.");
        return false;
    }
    if (raw) {
        // Unedited, the raw view is the file exactly as it is, comments and all.
        m_rawText = dirty() || !m_exists ? QString::fromUtf8(text()) : QString::fromUtf8(m_loaded);
        m_raw = true;
        return true;
    }
    const QByteArray typed = m_rawText.toUtf8();
    bool readable = false;
    if (m_kind == FileKind::Statement) {
        const StatementReading reading = readStatement(typed, m_path);
        readable = reading.parsed && reading.lossless;
        if (readable) {
            m_statement = reading.document;
            m_body = QString::fromUtf8(reading.document.body);
        }
    } else {
        const ProofReading reading = readProof(typed, m_path);
        readable = reading.parsed && reading.lossless;
        if (readable) {
            m_proof = reading.document;
            m_body = QString::fromUtf8(reading.document.body);
        }
    }
    if (!readable) {
        if (why)
            *why = QStringLiteral("Loom can't read every field of this front matter, so the form can't show it. Fix it here first: "
                                  "the Problems panel says what's wrong.");
        return false;
    }
    m_raw = false;
    return true;
}

void EditBuffer::setStatement(const Statement& statement)
{
    m_statement = statement;
}

void EditBuffer::setProof(const Proof& proof)
{
    m_proof = proof;
}

QString EditBuffer::editorText() const
{
    return m_raw ? m_rawText : m_body;
}

void EditBuffer::setEditorText(const QString& text)
{
    if (m_kind == FileKind::Stub)
        return;
    if (m_raw)
        m_rawText = text;
    else
        m_body = text;
}

void EditBuffer::setOutside(Outside outside, const QByteArray& bytes)
{
    m_outside = outside;
    m_outsideBytes = bytes;
}

void EditBuffer::setRecovery(Recovery recovery, const QDateTime& when)
{
    m_recovery = recovery;
    m_recoveredAt = when;
}

void EditBuffer::setRefusal(const QString& message, const std::vector<Problem>& problems)
{
    m_refusalMessage = message;
    m_refusalProblems = problems;
}

} // namespace loom
