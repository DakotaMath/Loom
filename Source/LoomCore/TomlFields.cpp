#include "LoomCore/TomlFields.h"

#include <algorithm>

namespace loom {

QString fromUtf8(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

int lineOf(const toml::source_region& region)
{
    return region.begin.line > 0 ? static_cast<int>(region.begin.line) : 1;
}

QString describeToml(const toml::node& node)
{
    switch (node.type()) {
    case toml::node_type::string:
        return node.as_string()->get().empty() ? QStringLiteral("an empty string") : QStringLiteral("a string");
    case toml::node_type::integer: return QStringLiteral("a whole number");
    case toml::node_type::floating_point: return QStringLiteral("a decimal number");
    case toml::node_type::boolean: return QStringLiteral("true or false");
    case toml::node_type::date: return QStringLiteral("a date");
    case toml::node_type::time: return QStringLiteral("a time");
    case toml::node_type::date_time: return QStringLiteral("a date and time");
    case toml::node_type::array: return QStringLiteral("a list");
    case toml::node_type::table: return QStringLiteral("a table");
    case toml::node_type::none: break;
    }
    return QStringLiteral("nothing");
}

void FieldChecker::add(const char* code, int line, const QString& message, Severity severity)
{
    problems.push_back({severity, QString::fromLatin1(code), file, line, message});
}

void FieldChecker::unknownFields(const toml::table& table, std::initializer_list<std::string_view> known,
                                 const QString& prefix, const QString& thing, QStringView fieldList)
{
    for (auto&& [key, node] : table) {
        if (std::find(known.begin(), known.end(), key.str()) != known.end())
            continue;
        add("unknown-field", line(key),
            QStringLiteral("`%1%2` isn't a field of %3. Is it misspelt? The fields are: %4.")
                .arg(prefix, fromUtf8(key.str()), thing, fieldList));
    }
}

void FieldChecker::missingField(const QString& field, int line, const QString& thing)
{
    add("missing-field", line, QStringLiteral("`%1` is missing; every %2 needs one.").arg(field, thing));
}

void FieldChecker::wrongType(const QString& field, const QString& expected, const toml::node& node)
{
    add("wrong-type", line(node), QStringLiteral("`%1` should be %2, not %3.").arg(field, expected, describeToml(node)));
}

std::optional<QString> FieldChecker::stringField(const toml::table& table, std::string_view key, const QString& field,
                                                 int tableLine, const QString& thing, Need need)
{
    const toml::node* node = table.get(key);
    if (!node) {
        if (need != Need::Optional)
            missingField(field, tableLine, thing);
        return std::nullopt;
    }
    const toml::value<std::string>* value = node->as_string();
    if (!value || (need == Need::RequiredNonEmpty && value->get().empty())) {
        wrongType(field, need == Need::RequiredNonEmpty ? QStringLiteral("a non-empty string") : QStringLiteral("a string"), *node);
        return std::nullopt;
    }
    return fromUtf8(value->get());
}

} // namespace loom
