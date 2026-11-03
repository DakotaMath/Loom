#include "LoomCore/TomlWriter.h"

namespace loom {

namespace {

/// Escapes text for the inside of a TOML basic string, single- or multi-line.
/// @param text        The text to escape.
/// @param keepNewline True in a multi-line string, where LF stays a real line break.
/// @returns The escaped text (TOML 1.0, "String": `\"`, `\\`, `\b`, `\t`, `\n`, `\f`, `\r`, and
///          `\uXXXX` for the other control characters). No side effects.
QString escapeBasic(const QString& text, bool keepNewline)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case u'"': out += u"\\\""; break;
        case u'\\': out += u"\\\\"; break;
        case u'\b': out += u"\\b"; break;
        case u'\t': out += u"\\t"; break;
        case u'\f': out += u"\\f"; break;
        case u'\r': out += u"\\r"; break;
        case u'\n': out += keepNewline ? u"\n" : u"\\n"; break;
        default:
            if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                out += u"\\u" + QString::number(c.unicode(), 16).toUpper().rightJustified(4, u'0');
            else
                out += c;
        }
    }
    return out;
}

} // namespace

QString tomlBasicString(const QString& text)
{
    return u'"' + escapeBasic(text, false) + u'"';
}

QString tomlMultilineString(const QString& text)
{
    QString body = text;
    body.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (!body.endsWith(u'\n'))
        body += u'\n';
    return u"\"\"\"\n" + escapeBasic(body, true) + u"\"\"\"";
}

QString tomlDate(const QDate& date)
{
    return date.toString(Qt::ISODate);
}

QString tomlStringArray(const QStringList& items)
{
    QStringList quoted;
    quoted.reserve(items.size());
    for (const QString& item : items)
        quoted << tomlBasicString(item);
    return u'[' + quoted.join(u", ") + u']';
}

} // namespace loom
