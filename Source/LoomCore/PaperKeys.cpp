#include "LoomCore/PaperKeys.h"

#include <algorithm>
#include <tuple>

namespace loom {

namespace {

/// The replacement for a letter that doesn't decompose, or nothing.
QString replacementOf(QChar c)
{
    switch (c.unicode()) {
    case 0x00DF: return QStringLiteral("ss"); // ß
    case 0x00E6: return QStringLiteral("ae"); // æ
    case 0x00C6: return QStringLiteral("Ae"); // Æ
    case 0x00F8: return QStringLiteral("o");  // ø
    case 0x00D8: return QStringLiteral("O");  // Ø
    case 0x0153: return QStringLiteral("oe"); // œ
    case 0x0152: return QStringLiteral("Oe"); // Œ
    case 0x0142: return QStringLiteral("l");  // ł
    case 0x0141: return QStringLiteral("L");  // Ł
    case 0x0111: return QStringLiteral("d");  // đ
    case 0x00F0: return QStringLiteral("d");  // ð
    case 0x0110: return QStringLiteral("D");  // Đ
    case 0x00D0: return QStringLiteral("D");  // Ð
    case 0x00FE: return QStringLiteral("th"); // þ
    case 0x00DE: return QStringLiteral("Th"); // Þ
    case 0x0131: return QStringLiteral("i");  // ı
    default: return {};
    }
}

/// Where a surname splits into parts: spaces, hyphens and apostrophes, including their
/// typographic forms (`’`, `ʼ`, the Unicode hyphens).
bool isSplit(QChar c)
{
    return c.isSpace() || c == u'-' || c == u'\'' || c == QChar(0x2019) || c == QChar(0x02BC) || c == QChar(0x2010)
        || c == QChar(0x2011);
}

/// Whether `c` is `a`–`z` or `A`–`Z`. No side effects.
bool isAsciiLetter(QChar c)
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

/// The year of a field holding a whole number or a date.
std::optional<int> yearOf(const PaperField* field)
{
    if (!field)
        return std::nullopt;
    if (const qint64* number = std::get_if<qint64>(&field->value))
        return int(*number);
    if (const QDate* date = std::get_if<QDate>(&field->value); date && date->isValid())
        return date->year();
    return std::nullopt;
}

/// The arXiv version number of a version: its `arxiv_version`, else the N of `arxiv-v<N>`.
qint64 arxivNumber(const PaperVersion& version)
{
    if (const PaperField* field = version.field(u"arxiv_version"); field && std::holds_alternative<qint64>(field->value))
        return std::get<qint64>(field->value);
    return version.id.mid(7).toLongLong();
}

/// The date posting order goes by: the earliest arXiv `posted`, else 1 January of the published year.
QDate postingDate(const Paper& paper)
{
    QDate earliest;
    for (const PaperVersion& version : paper.versions) {
        if (version.kind != u"arxiv")
            continue;
        if (const PaperField* posted = version.field(u"posted")) {
            if (const QDate* date = std::get_if<QDate>(&posted->value); date && date->isValid() && (!earliest.isValid() || *date < earliest))
                earliest = *date;
        }
    }
    if (earliest.isValid())
        return earliest;
    if (const PaperVersion* published = paper.version(QStringLiteral("published"))) {
        if (const std::optional<int> year = yearOf(published->field(u"year")))
            return QDate(*year, 1, 1);
    }
    return {};
}

} // namespace

QString camelSurname(const QString& last)
{
    QString plain;
    for (const QChar c : last.normalized(QString::NormalizationForm_KD)) {
        if (c.category() == QChar::Mark_NonSpacing || c.category() == QChar::Mark_SpacingCombining
            || c.category() == QChar::Mark_Enclosing)
            continue;
        const QString replacement = replacementOf(c);
        plain += replacement.isEmpty() ? QString(c) : replacement;
    }

    QString out;
    bool partStart = true;
    for (const QChar c : plain) {
        if (isSplit(c)) {
            partStart = true;
            continue;
        }
        // The first letter of each part; a part starting with, say, `(` capitalises its first letter.
        if (partStart && c.isLetter()) {
            out += c.toUpper();
            partStart = false;
        } else {
            out += c;
        }
    }
    out.removeIf([](QChar c) { return !isAsciiLetter(c); });
    return out;
}

std::optional<int> keyYear(const Paper& paper)
{
    if (const PaperVersion* published = paper.version(QStringLiteral("published"))) {
        if (const std::optional<int> year = yearOf(published->field(u"year")))
            return year;
    }
    const PaperVersion* lowest = nullptr;
    for (const PaperVersion& version : paper.versions) {
        if (version.kind == u"arxiv" && version.field(u"posted") && (!lowest || arxivNumber(version) < arxivNumber(*lowest)))
            lowest = &version;
    }
    if (lowest) {
        if (const std::optional<int> year = yearOf(lowest->field(u"posted")))
            return year;
    }
    for (const PaperVersion& version : paper.versions) {
        if (const std::optional<int> year = yearOf(version.field(u"date")))
            return year;
    }
    return std::nullopt;
}

KeyBase baseKey(const Paper& paper)
{
    const std::vector<PersonName> names = paper.keyNames();
    if (names.empty())
        return {{}, QStringLiteral("Can't make a key for this paper: it has no authors or editors.")};
    QStringList surnames;
    for (const PersonName& name : names) {
        const QString camel = camelSurname(name.last);
        if (camel.isEmpty())
            return {{}, QStringLiteral("Can't make a key from the surname `%1`: write it in Latin letters, as the paper prints it.").arg(name.last)};
        surnames << camel;
    }
    QString key;
    if (surnames.size() <= 3) {
        key = surnames.join(QString());
    } else {
        key = surnames.first();
        for (qsizetype i = 1; i < surnames.size(); ++i)
            key += surnames[i].left(2);
    }
    const std::optional<int> year = keyYear(paper);
    if (!year)
        return {{}, QStringLiteral("Can't make a key for this paper: it has no year. Add a published year, an arXiv posting date, or a date.")};
    return {key + QStringLiteral("%1").arg(((*year % 100) + 100) % 100, 2, 10, QChar(u'0')), {}};
}

bool keyFitsBase(const QString& key, const QString& base)
{
    if (key == base)
        return true;
    return key.size() == base.size() + 1 && key.startsWith(base) && key.back() >= u'a' && key.back() <= u'z';
}

bool postedBefore(const Paper& a, const Paper& b)
{
    const QDate da = postingDate(a);
    const QDate db = postingDate(b);
    if (da.isValid() != db.isValid())
        return da.isValid();
    if (da != db)
        return da < db;
    const QString ta = a.work.title ? a.work.title->text() : QString();
    const QString tb = b.work.title ? b.work.title->text() : QString();
    return ta < tb;
}

KeyAssignment assignKey(const Paper& paper, const std::vector<KeyedPaper>& others)
{
    KeyAssignment result;
    const KeyBase base = baseKey(paper);
    if (base.key.isEmpty()) {
        result.problem = base.problem;
        return result;
    }

    // The letters already used with this base, as keys or old keys, and who holds the base itself.
    char highest = 0;
    bool baseTaken = false;
    const KeyedPaper* exact = nullptr;
    for (const KeyedPaper& other : others) {
        QStringList names = other.aliases;
        names.prepend(other.key);
        for (const QString& name : names) {
            if (name == base.key)
                baseTaken = true;
            else if (keyFitsBase(name, base.key))
                highest = std::max(highest, char(name.back().unicode()));
        }
        if (other.key == base.key)
            exact = &other;
    }
    if (!baseTaken && highest == 0) {
        result.key = base.key;
        return result;
    }

    const auto lettered = [&](char letter) { return base.key + QChar::fromLatin1(letter); };
    if (exact && highest == 0) {
        // The first time: both get letters in posting order.
        const bool mineFirst = exact->paper && postedBefore(paper, *exact->paper);
        result.renames.push_back({exact->key, lettered(mineFirst ? 'b' : 'a')});
        result.key = lettered(mineFirst ? 'a' : 'b');
        return result;
    }
    char next = highest == 0 ? 'a' : char(highest + 1);
    if (exact) {
        if (next > 'z') {
            result.problem = QStringLiteral("Can't make a key for this paper: every letter after `%1` is taken.").arg(base.key);
            return result;
        }
        result.renames.push_back({exact->key, lettered(next)});
        ++next;
    }
    if (next > 'z') {
        result.renames.clear();
        result.problem = QStringLiteral("Can't make a key for this paper: every letter after `%1` is taken.").arg(base.key);
        return result;
    }
    result.key = lettered(next);
    return result;
}

} // namespace loom
