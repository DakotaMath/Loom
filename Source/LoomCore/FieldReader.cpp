#include "LoomCore/FieldReader.h"

#include "LoomCore/FrontMatter.h"
#include "LoomCore/Ids.h"

#include <QSet>

#include <string>

namespace loom {

QString quotedList(const QStringList& values)
{
    QStringList quoted;
    for (const QString& value : values)
        quoted << u'`' + value + u'`';
    return quoted.join(u", ");
}

std::optional<toml::table> parsePlainToml(const QByteArray& bytes, FieldChecker& checker)
{
    if (const std::optional<qsizetype> bad = firstInvalidUtf8(bytes)) {
        checker.add("not-utf8", 1,
            QStringLiteral("This file isn't valid UTF-8 (the first bad byte is at offset %1). Save it as UTF-8.").arg(*bad));
        return std::nullopt;
    }
    try {
        return toml::parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), checker.file.toStdString());
    } catch (const toml::parse_error& error) {
        QString description = fromUtf8(error.description());
        while (description.endsWith(u'.'))
            description.chop(1);
        checker.add("toml-syntax", lineOf(error.source()) + checker.lineOffset,
                    QStringLiteral("This isn't valid TOML: %1.").arg(description));
        return std::nullopt;
    }
}

FieldReader::FieldReader(const QString& path, QString what, int lineOffset)
    : checker{path, lineOffset, {}}, thing(std::move(what))
{
}

void FieldReader::unknownFields(const toml::table& table, std::initializer_list<std::string_view> known, QStringView fieldList)
{
    const std::size_t before = checker.problems.size();
    checker.unknownFields(table, known, QString(), u"a " + thing, fieldList);
    lost = lost || checker.problems.size() != before;
}

std::optional<QString> FieldReader::string(const toml::table& table, const char* key, Need need, int missingLine)
{
    const QString field = QString::fromLatin1(key);
    const toml::node* node = table.get(key);
    if (!node) {
        if (need == Need::Required)
            checker.missingField(field, missingLine, thing);
        return std::nullopt;
    }
    lines.fields.insert(field, checker.line(*node));
    const toml::value<std::string>* value = node->as_string();
    if (!value) {
        checker.wrongType(field, QStringLiteral("a string"), *node);
        lost = true;
        return std::nullopt;
    }
    // An empty value carries nothing, so leaving it out on a rewrite loses nothing: not `lost`.
    if (checker.reportEmpty(field, *node, thing, need))
        return std::nullopt;
    return fromUtf8(value->get());
}

std::optional<QStringList> FieldReader::list(const toml::table& table, const char* key, const QString& expected, bool required)
{
    const QString field = QString::fromLatin1(key);
    const toml::node* node = table.get(key);
    if (!node) {
        if (required)
            checker.missingField(field, 1, thing);
        return std::nullopt;
    }
    lines.fields.insert(field, checker.line(*node));
    const toml::array* array = node->as_array();
    if (!array) {
        checker.wrongType(field, expected, *node);
        lost = true;
        return std::nullopt;
    }
    QStringList values;
    QList<int>& elementLines = lines.elements[field];
    for (const toml::node& element : *array) {
        const int line = checker.line(element);
        const toml::value<std::string>* value = element.as_string();
        if (!value) {
            checker.add("wrong-type", line,
                QStringLiteral("`%1` should be %2, not a list holding %3.").arg(field, expected, describeToml(element)));
            lost = true;
            continue;
        }
        values << fromUtf8(value->get());
        elementLines << line;
    }
    return values;
}

QDate FieldReader::date(const toml::table& table, const char* key, bool required, int missingLine)
{
    const QString field = QString::fromLatin1(key);
    const toml::node* node = table.get(key);
    if (!node) {
        if (required)
            checker.missingField(field, missingLine, thing);
        return {};
    }
    lines.fields.insert(field, checker.line(*node));
    if (checker.reportEmpty(field, *node, thing, required ? Need::Required : Need::Naming))
        return {};
    const toml::value<toml::date>* value = node->as_date();
    if (!value) {
        checker.wrongType(field, QStringLiteral("a date"), *node);
        lost = true;
        return {};
    }
    // toml++ reads years 0000 to 9999, and QDate has no year 0. Such a date would otherwise be
    // dropped without a word, and lost on the next rewrite.
    const toml::date date = value->get();
    const QDate out(date.year, date.month, date.day);
    if (!out.isValid()) {
        const QString text = QString::asprintf("%04d-%02d-%02d", int(date.year), int(date.month), int(date.day));
        checker.add("bad-value", checker.line(*node),
            QStringLiteral("`%1` can't be `%2`; it must be one of the dates from 0001-01-01 on.").arg(field, text));
        lost = true;
    }
    return out;
}

std::optional<qint64> FieldReader::integer(const toml::table& table, const char* key, bool required, int missingLine)
{
    const QString field = QString::fromLatin1(key);
    const toml::node* node = table.get(key);
    if (!node) {
        if (required)
            checker.missingField(field, missingLine, thing);
        return std::nullopt;
    }
    lines.fields.insert(field, checker.line(*node));
    const toml::value<int64_t>* value = node->as_integer();
    if (!value) {
        checker.wrongType(field, QStringLiteral("a whole number"), *node);
        lost = true;
        return std::nullopt;
    }
    return value->get();
}

void FieldReader::badValue(const QString& field, const QString& value, const QString& allowed)
{
    checker.add("bad-value", lines.of(field), QStringLiteral("`%1` can't be `%2`; it must be one of %3.").arg(field, value, allowed));
}

void FieldReader::repeats(const QString& field, const QStringList& values)
{
    QSet<QString> seen;
    for (qsizetype i = 0; i < values.size(); ++i) {
        if (seen.contains(values[i]))
            checker.add("bad-value", lines.of(field, i), QStringLiteral("`%1` can't list `%2` twice.").arg(field, values[i]));
        seen.insert(values[i]);
    }
}

void FieldReader::tagSyntax(const QString& value, int line)
{
    QString message = QStringLiteral("`%1` isn't a valid tag: four characters from 0–9 and A–Z without I, L, O and U.").arg(value);
    if (const std::optional<QString> normalised = normaliseTag(value))
        message += QStringLiteral(" Did you mean `%1`?").arg(*normalised);
    checker.add("id-syntax", line, message);
}

void FieldReader::aliases(const QStringList& aliases)
{
    for (qsizetype i = 0; i < aliases.size(); ++i) {
        const QString why = aliasProblem(aliases[i]);
        if (!why.isEmpty())
            checker.add("alias-invalid", lines.of(QStringLiteral("aliases"), i),
                QStringLiteral("`%1` can't be an alias: %2.").arg(aliases[i], why));
    }
}

} // namespace loom
