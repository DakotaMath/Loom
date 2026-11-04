#include "LoomCore/Ids.h"

#include <QList>
#include <QStringView>

#include <limits>

namespace loom {

namespace {

constexpr qsizetype tagLength = 4;
constexpr qsizetype alphabetSize = sizeof(tagAlphabet) - 1;

bool inAlphabet(QChar c)
{
    return c.unicode() < 0x80 && QByteArrayView(tagAlphabet).contains(static_cast<char>(c.unicode()));
}

bool isLowerLetters(QStringView text)
{
    if (text.isEmpty())
        return false;
    for (const QChar c : text) {
        if (c < u'a' || c > u'z')
            return false;
    }
    return true;
}

} // namespace

bool isTag(const QString& text)
{
    if (text.size() != tagLength)
        return false;
    for (const QChar c : text) {
        if (!inAlphabet(c))
            return false;
    }
    return true;
}

std::optional<QString> normaliseTag(const QString& typed)
{
    if (typed.size() != tagLength)
        return std::nullopt;
    QString tag;
    for (const QChar typedChar : typed) {
        QChar c = typedChar.toUpper();
        if (c == u'O')
            c = u'0';
        else if (c == u'I' || c == u'L')
            c = u'1';
        if (!inAlphabet(c))
            return std::nullopt;
        tag += c;
    }
    return tag;
}

std::optional<QString> generateTag(const std::function<bool(const QString&)>& isUsed, QRandomGenerator& random)
{
    for (int draw = 0; draw < tagDraws; ++draw) {
        QString tag;
        bool allDigits = true;
        for (qsizetype i = 0; i < tagLength; ++i) {
            const char c = tagAlphabet[random.bounded(static_cast<quint32>(alphabetSize))];
            allDigits = allDigits && c >= '0' && c <= '9';
            tag += QLatin1Char(c);
        }
        // All-digit tags read as numbers ("Theorem 2024"), so they're never generated.
        if (!allDigits && !isUsed(tag))
            return tag;
    }
    return std::nullopt;
}

QString proofLetters(qint64 number)
{
    // Bijective base 26: there is no zero digit, so `z` is followed by `aa`.
    QString letters;
    while (number > 0) {
        --number;
        letters.prepend(QChar(u'a' + static_cast<char16_t>(number % 26)));
        number /= 26;
    }
    return letters;
}

qint64 proofNumber(const QString& letters)
{
    if (!isLowerLetters(letters))
        return 0;
    qint64 number = 0;
    for (const QChar c : letters) {
        if (number > (std::numeric_limits<qint64>::max() - 26) / 26)
            return 0;
        number = number * 26 + (c.unicode() - u'a' + 1);
    }
    return number;
}

std::optional<ProofIdParts> splitProofId(const QString& text)
{
    const qsizetype dot = text.indexOf(u'.');
    if (dot != tagLength)
        return std::nullopt;
    const QString tag = text.first(dot);
    const QString letters = text.mid(dot + 1);
    if (!isTag(tag) || !isLowerLetters(letters))
        return std::nullopt;
    return ProofIdParts{tag, letters};
}

std::optional<QString> normaliseProofId(const QString& typed)
{
    const qsizetype dot = typed.indexOf(u'.');
    if (dot != tagLength)
        return std::nullopt;
    const std::optional<QString> tag = normaliseTag(typed.first(dot));
    const QString letters = typed.mid(dot + 1).toLower();
    if (!tag || !isLowerLetters(letters))
        return std::nullopt;
    return *tag + u'.' + letters;
}

QString aliasProblem(const QString& alias)
{
    if (alias.isEmpty())
        return QStringLiteral("it is empty");
    for (const QChar c : alias) {
        if (c.isSpace() || QStringView(u"{}[],\"").contains(c))
            return QStringLiteral("it contains whitespace or one of {}[],\"");
    }
    if (normaliseTag(alias))
        return QStringLiteral("it is shaped like a tag");
    // "1–100 characters": counted as characters a person sees, so a surrogate pair is one.
    if (alias.toUcs4().size() > 100)
        return QStringLiteral("it's over 100 characters");
    return {};
}

} // namespace loom
