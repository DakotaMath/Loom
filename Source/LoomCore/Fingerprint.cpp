#include "LoomCore/Fingerprint.h"

#include "LoomCore/FrontMatter.h"

#include <QCryptographicHash>

#include <algorithm>

namespace loom {

namespace {

/// Appends one section: the label line, the length line, the text and `\n`.
void appendSection(QByteArray& out, const QByteArray& label, const QByteArray& text)
{
    out += label + '\n' + QByteArray::number(text.size()) + '\n' + text + '\n';
}

} // namespace

QByteArray normaliseForFingerprint(const QByteArray& body)
{
    const QByteArray lf = crlfToLf(body);
    QByteArray out;
    out.reserve(lf.size());
    qsizetype start = 0;
    while (start <= lf.size()) {
        qsizetype end = lf.indexOf('\n', start);
        const bool last = end < 0;
        if (last)
            end = lf.size();
        qsizetype trimmed = end;
        while (trimmed > start && (lf[trimmed - 1] == ' ' || lf[trimmed - 1] == '\t'))
            --trimmed;
        out += lf.mid(start, trimmed - start);
        if (last)
            break;
        out += '\n';
        start = end + 1;
    }
    while (out.endsWith('\n'))
        out.chop(1);
    return out;
}

QByteArray fingerprintBytes(const FingerprintInput& input)
{
    QByteArray out = "loom-fingerprint-v1\n";
    appendSection(out, "statement " + input.statementTag.toUtf8(), normaliseForFingerprint(input.statementBody));

    // By tag in byte order: the order `premises` lists them in isn't part of what was read.
    std::vector<FingerprintPremise> premises = input.premises;
    std::stable_sort(premises.begin(), premises.end(),
                     [](const FingerprintPremise& a, const FingerprintPremise& b) { return a.tag.toUtf8() < b.tag.toUtf8(); });
    for (const FingerprintPremise& premise : premises)
        appendSection(out, "premise " + premise.tag.toUtf8(), premise.body ? normaliseForFingerprint(*premise.body) : QByteArray());

    appendSection(out, "proof", normaliseForFingerprint(input.proofBody));
    return out;
}

QString fingerprint(const FingerprintInput& input)
{
    const QByteArray hash = QCryptographicHash::hash(fingerprintBytes(input), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex().left(12));
}

bool isFingerprint(const QString& text)
{
    return text.size() == 12
        && std::all_of(text.begin(), text.end(), [](QChar c) { return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f'); });
}

} // namespace loom
