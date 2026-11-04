#include "LoomCore/FrontMatter.h"

namespace loom {

namespace {

constexpr QByteArrayView fence = "+++";
constexpr QByteArrayView byteOrderMark = "\xEF\xBB\xBF";

/// The file line holding byte `offset` of `bytes`, counting from 1.
int lineAt(QByteArrayView bytes, qsizetype offset)
{
    return static_cast<int>(bytes.first(offset).count('\n')) + 1;
}

} // namespace

std::optional<qsizetype> firstInvalidUtf8(QByteArrayView bytes)
{
    const qsizetype size = bytes.size();
    qsizetype i = 0;
    while (i < size) {
        const auto lead = static_cast<unsigned char>(bytes[i]);
        if (lead < 0x80) {
            ++i;
            continue;
        }
        // The length of the sequence, and the allowed range of its second byte, which rules out
        // overlong forms, surrogates and code points above U+10FFFF (RFC 3629, section 4).
        qsizetype length = 0;
        unsigned char low = 0x80;
        unsigned char high = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            if (lead == 0xE0)
                low = 0xA0;
            else if (lead == 0xED)
                high = 0x9F;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            if (lead == 0xF0)
                low = 0x90;
            else if (lead == 0xF4)
                high = 0x8F;
        } else {
            return i;
        }
        for (qsizetype k = 1; k < length; ++k) {
            if (i + k >= size)
                return i + k;
            const auto c = static_cast<unsigned char>(bytes[i + k]);
            const unsigned char min = k == 1 ? low : 0x80;
            const unsigned char max = k == 1 ? high : 0xBF;
            if (c < min || c > max)
                return k == 1 ? i : i + k;
        }
        i += length;
    }
    return std::nullopt;
}

QByteArray crlfToLf(QByteArray bytes)
{
    return bytes.replace("\r\n", "\n");
}

bool isBlankBody(const QByteArray& body)
{
    return QString::fromUtf8(body).trimmed().isEmpty();
}

FrontMatterSplit splitFrontMatter(const QByteArray& bytes, const QString& path)
{
    FrontMatterSplit split;

    if (const std::optional<qsizetype> bad = firstInvalidUtf8(bytes)) {
        split.problems.push_back({Severity::Error, QStringLiteral("not-utf8"), path, lineAt(bytes, *bad),
            QStringLiteral("This file isn't valid UTF-8 (the first bad byte is at offset %1). Save it as UTF-8.").arg(*bad)});
        return split;
    }

    QByteArray text = bytes;
    if (text.startsWith(byteOrderMark)) {
        text.remove(0, byteOrderMark.size());
        split.problems.push_back({Severity::Warning, QStringLiteral("bom"), path, 1,
            QStringLiteral("This file starts with a byte-order mark. Loom will drop it the next time it saves the file.")});
    }
    text = crlfToLf(std::move(text));

    // The first line must be exactly `+++`, ended by LF (a file that is only `+++` is unclosed).
    const qsizetype firstEnd = text.indexOf('\n');
    const QByteArrayView firstLine = firstEnd < 0 ? QByteArrayView(text) : QByteArrayView(text).first(firstEnd);
    if (firstLine != fence) {
        split.problems.push_back({Severity::Error, QStringLiteral("front-matter-missing"), path, 1,
            QStringLiteral("The first line should be `+++`: Loom files start with front matter between two `+++` lines.")});
        return split;
    }

    // The front matter ends at the next line that is exactly `+++`; only the first one counts.
    qsizetype lineStart = firstEnd < 0 ? text.size() : firstEnd + 1;
    int line = 2;
    while (lineStart < text.size()) {
        const qsizetype lineEnd = text.indexOf('\n', lineStart);
        const qsizetype end = lineEnd < 0 ? text.size() : lineEnd;
        if (QByteArrayView(text).sliced(lineStart, end - lineStart) == fence) {
            split.found = true;
            split.toml = text.sliced(firstEnd + 1, lineStart - firstEnd - 1);
            split.body = lineEnd < 0 ? QByteArray() : text.sliced(lineEnd + 1);
            split.bodyLine = line + 1;
            return split;
        }
        if (lineEnd < 0)
            break;
        lineStart = lineEnd + 1;
        ++line;
    }
    split.problems.push_back({Severity::Error, QStringLiteral("front-matter-unclosed"), path, 1,
        QStringLiteral("The front matter starting on line 1 is never closed by a `+++` line.")});
    return split;
}

QByteArray joinFrontMatter(const QByteArray& toml, const QByteArray& body)
{
    return "+++\n" + toml + "+++\n" + body;
}

} // namespace loom
