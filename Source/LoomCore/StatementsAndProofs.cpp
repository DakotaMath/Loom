#include "LoomCore/StatementsAndProofs.h"

#include "LoomCore/FieldReader.h"
#include "LoomCore/FrontMatter.h"
#include "LoomCore/Ids.h"
#include "LoomCore/TomlFields.h"
#include "LoomCore/TomlWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace loom {

namespace {

constexpr auto statementFieldList =
    u"id, kind, title, tags, stated_using, aliases, origin, superseded_by, refuted_by, refuted_on, created, verified";
constexpr auto proofFieldList = u"id, proves, premises, level, created, verified";
constexpr auto verboseFieldList = u"proof, fingerprint, created";

/// Splits the file and parses its front matter.
/// @returns The TOML table, or nothing (with the problem recorded) if there isn't one.
std::optional<toml::table> parseFrontMatter(const FrontMatterSplit& split, FieldReader& reader)
{
    for (const Problem& problem : split.problems)
        reader.checker.problems.push_back(problem);
    if (!split.found)
        return std::nullopt;
    try {
        return toml::parse(std::string_view(split.toml.constData(), static_cast<std::size_t>(split.toml.size())),
                           reader.checker.file.toStdString());
    } catch (const toml::parse_error& error) {
        QString description = fromUtf8(error.description());
        while (description.endsWith(u'.'))
            description.chop(1);
        reader.checker.add("toml-syntax", lineOf(error.source()) + reader.checker.lineOffset,
                           QStringLiteral("This isn't valid TOML: %1.").arg(description));
        return std::nullopt;
    }
}

/// Fills in the parts of a reading every document shares, once its fields are read.
template <typename Document>
void finish(DocumentReading<Document>& reading, FieldReader& reader, const FrontMatterSplit& split,
            const QString& name, bool parsed)
{
    reading.parsed = parsed;
    reading.document.body = split.body;
    reading.bodyLine = split.found ? split.bodyLine : 0;
    if (parsed && isBlankBody(split.body))
        reader.checker.add("body-empty", split.bodyLine, QStringLiteral("%1 has no text below its front matter.").arg(name));
    reading.lossless = parsed && !reader.lost;
    reading.lines = std::move(reader.lines);
    reading.problems = std::move(reader.checker.problems);
    sortProblems(reading.problems);
}

/// The file name without its folder and without `suffix`, e.g. `4KX7.a` for `Proofs/4KX7/4KX7.a.tex`.
QString baseName(const QString& path, QStringView suffix)
{
    QString name = QFileInfo(path).fileName();
    if (name.endsWith(suffix))
        name.chop(suffix.size());
    return name;
}

/// `key = value` lines, appended in order. The callers give the fields in the spec's order
/// (Specs/m1-core.md, "Front matter": Loom's order, not toml++'s alphabetical one), so the
/// writer only formats and leaves out what has no value.
struct Writer {
    QString text;   ///< The lines so far, each ending in a line break.

    /// Appends `key = "value"`, left out when the value is empty: an empty optional field means
    /// the same as a missing one, and an empty required one was reported on reading.
    void string(const char* key, const QString& value)
    {
        if (!value.isEmpty())
            text += QStringLiteral("%1 = %2\n").arg(QLatin1StringView(key), tomlBasicString(value));
    }
    /// Appends `key = ["a", "b"]`, left out when empty unless `always`: a proof's `premises` is
    /// required, so its `premises = []` must be written.
    void list(const char* key, const QStringList& values, bool always = false)
    {
        if (always || !values.isEmpty())
            text += QStringLiteral("%1 = %2\n").arg(QLatin1StringView(key), tomlStringArray(values));
    }
    /// Appends `key = YYYY-MM-DD`, left out when the date is invalid (an optional date not set).
    void date(const char* key, const QDate& value)
    {
        if (value.isValid())
            text +=QStringLiteral("%1 = %2\n").arg(QLatin1StringView(key), tomlDate(value));
    }
};

} // namespace

bool isProvableKind(const QString& kind)
{
    return kind == u"lemma" || kind == u"proposition" || kind == u"theorem" || kind == u"corollary" || kind == u"conjecture";
}

int FieldLines::of(const QString& field, qsizetype element) const
{
    if (element >= 0) {
        const QList<int> lines = elements.value(field);
        if (element < lines.size())
            return lines[element];
    }
    return fields.value(field, 1);
}

StatementReading readStatement(const QByteArray& bytes, const QString& path)
{
    StatementReading reading;
    FieldReader reader(path, QStringLiteral("statement"), 1);
    const QString name = baseName(path, u".tex");
    const FrontMatterSplit split = splitFrontMatter(bytes, path);
    const std::optional<toml::table> table = parseFrontMatter(split, reader);
    if (!table) {
        finish(reading, reader, split, name, false);
        return reading;
    }

    Statement& s = reading.document;
    reader.unknownFields(*table, {"id", "kind", "title", "tags", "stated_using", "aliases", "origin", "superseded_by",
                                  "refuted_by", "refuted_on", "created", "verified"}, statementFieldList);
    s.id = reader.string(*table, "id", Need::Required).value_or(QString());
    s.kind = reader.string(*table, "kind", Need::Required).value_or(QString());
    s.title = reader.string(*table, "title", Need::Required).value_or(QString());
    s.tags = reader.list(*table, "tags", QStringLiteral("a list of tags"), false).value_or(QStringList());
    s.statedUsing = reader.list(*table, "stated_using", QStringLiteral("a list of tags"), false).value_or(QStringList());
    s.aliases = reader.list(*table, "aliases", QStringLiteral("a list of aliases"), false).value_or(QStringList());
    const std::optional<QString> origin = reader.string(*table, "origin", Need::Naming);
    s.origin = origin.value_or(QString());
    s.supersededBy = reader.string(*table, "superseded_by", Need::Naming).value_or(QString());
    s.refutedBy = reader.string(*table, "refuted_by", Need::Naming).value_or(QString());
    s.refutedOn = reader.date(*table, "refuted_on", false);
    s.created = reader.date(*table, "created", true);
    s.verified = reader.date(*table, "verified", false);

    if (!s.id.isEmpty()) {
        if (!isTag(s.id))
            reader.tagSyntax(s.id, reader.lines.of(QStringLiteral("id")));
        else if (s.id != name)
            reader.checker.add("id-mismatch", reader.lines.of(QStringLiteral("id")),
                QStringLiteral("The id is `%1` but the file is named `%2`; they must match.").arg(s.id, name));
    }
    if (!s.kind.isEmpty() && !statementKinds.contains(s.kind))
        reader.badValue(QStringLiteral("kind"), s.kind, quotedList(statementKinds));
    if (s.title.contains(u'\n') || s.title.contains(u'\r'))
        reader.checker.add("wrong-type", reader.lines.of(QStringLiteral("title")),
            QStringLiteral("`title` should be a string on one line, not a string with line breaks."));
    reader.repeats(QStringLiteral("tags"), s.tags);
    reader.repeats(QStringLiteral("stated_using"), s.statedUsing);
    reader.repeats(QStringLiteral("aliases"), s.aliases);
    reader.aliases(s.aliases);
    // Anchored at the very end: Qt's `$` also matches before a final line break.
    static const QRegularExpression originRule(QRegularExpression::anchoredPattern(QStringLiteral("new|spec:.+|import:.+")),
                                               QRegularExpression::DotMatchesEverythingOption);
    if (origin && !originRule.match(s.origin).hasMatch())
        reader.badValue(QStringLiteral("origin"), s.origin, QStringLiteral("`new`, `spec:<spec-id>`, `import:<path>`"));
    // Both or neither (Specs/m1-core.md, D4). A field dropped for its type was reported already.
    const bool hasRefutedBy = table->contains("refuted_by");
    const bool hasRefutedOn = table->contains("refuted_on");
    if (hasRefutedBy != hasRefutedOn) {
        const QString has = hasRefutedBy ? QStringLiteral("refuted_by") : QStringLiteral("refuted_on");
        const QString lacks = hasRefutedBy ? QStringLiteral("refuted_on") : QStringLiteral("refuted_by");
        reader.checker.add("refuted-incomplete", reader.lines.of(has),
            QStringLiteral("%1 has `%2` but not `%3`. Refuting a statement needs both: the proof and the date Dakota accepted it.")
                .arg(name, has, lacks));
    }

    finish(reading, reader, split, name, true);
    return reading;
}

ProofReading readProof(const QByteArray& bytes, const QString& path)
{
    ProofReading reading;
    FieldReader reader(path, QStringLiteral("proof"), 1);
    const QString name = baseName(path, u".tex");
    const FrontMatterSplit split = splitFrontMatter(bytes, path);
    const std::optional<toml::table> table = parseFrontMatter(split, reader);
    if (!table) {
        finish(reading, reader, split, name, false);
        return reading;
    }

    Proof& p = reading.document;
    reader.unknownFields(*table, {"id", "proves", "premises", "level", "created", "verified"}, proofFieldList);
    p.id = reader.string(*table, "id", Need::Required).value_or(QString());
    p.proves = reader.string(*table, "proves", Need::Required).value_or(QString());
    p.premises = reader.list(*table, "premises", QStringLiteral("a list of tags"), true).value_or(QStringList());
    p.level = reader.string(*table, "level", Need::Required).value_or(QString());
    p.created = reader.date(*table, "created", true);
    p.verified = reader.date(*table, "verified", false);

    // The id must equal the file name, which must start with the folder's tag.
    const QString folder = QFileInfo(QFileInfo(path).path()).fileName();
    const std::optional<ProofIdParts> parts = splitProofId(name);
    if (!p.id.isEmpty() && (p.id != name || !parts || parts->tag != folder))
        reader.checker.add("proof-id-mismatch", reader.lines.of(QStringLiteral("id")),
            QStringLiteral("The proof's id `%1` doesn't match its file name `%2`, or doesn't start with its folder's tag.").arg(p.id, name));
    if (!p.level.isEmpty() && !proofLevels.contains(p.level))
        reader.badValue(QStringLiteral("level"), p.level, quotedList(proofLevels));

    QSet<QString> seen;
    for (qsizetype i = 0; i < p.premises.size(); ++i) {
        const QString& premise = p.premises[i];
        const int line = reader.lines.of(QStringLiteral("premises"), i);
        if (!p.proves.isEmpty() && premise == p.proves)
            reader.checker.add("premise-self", line,
                QStringLiteral("%1 uses %2, the statement it proves, as a premise.").arg(name, premise));
        else if (seen.contains(premise))
            reader.checker.add("premise-duplicate", line,
                QStringLiteral("%1 is listed twice in the premises of %2.").arg(premise, name));
        seen.insert(premise);
    }

    finish(reading, reader, split, name, true);
    return reading;
}

VerboseReading readVerboseProof(const QByteArray& bytes, const QString& path)
{
    VerboseReading reading;
    FieldReader reader(path, QStringLiteral("verbose proof"), 1);
    const QString proofId = baseName(path, u".verbose.tex");
    const FrontMatterSplit split = splitFrontMatter(bytes, path);
    const std::optional<toml::table> table = parseFrontMatter(split, reader);
    if (!table) {
        finish(reading, reader, split, proofId + u".verbose", false);
        return reading;
    }

    VerboseProof& v = reading.document;
    reader.unknownFields(*table, {"proof", "fingerprint", "created"}, verboseFieldList);
    v.proof = reader.string(*table, "proof", Need::Required).value_or(QString());
    v.fingerprint = reader.string(*table, "fingerprint", Need::Required).value_or(QString());
    v.created = reader.date(*table, "created", true);

    if (!v.proof.isEmpty() && v.proof != proofId)
        reader.badValue(QStringLiteral("proof"), v.proof, u'`' + proofId + u'`');
    // The fingerprint's form (Specs/m1-core.md, D2: 12 hex characters of SHA-256, lowercase),
    // anchored at the very end: Qt's `$` also matches before a final line break.
    static const QRegularExpression fingerprintRule(QRegularExpression::anchoredPattern(QStringLiteral("[0-9a-f]{12}")));
    if (!v.fingerprint.isEmpty() && !fingerprintRule.match(v.fingerprint).hasMatch())
        reader.checker.add("wrong-type", reader.lines.of(QStringLiteral("fingerprint")),
            QStringLiteral("`fingerprint` should be 12 characters from 0–9 and a–f, not `%1`.").arg(v.fingerprint));

    finish(reading, reader, split, proofId + u".verbose", true);
    return reading;
}

QByteArray formatStatement(const Statement& s)
{
    Writer w;
    w.string("id", s.id);
    w.string("kind", s.kind);
    w.string("title", s.title);
    w.list("tags", s.tags);
    w.list("stated_using", s.statedUsing);
    w.list("aliases", s.aliases);
    w.string("origin", s.origin);
    w.string("superseded_by", s.supersededBy);
    w.string("refuted_by", s.refutedBy);
    w.date("refuted_on", s.refutedOn);
    w.date("created", s.created);
    w.date("verified", s.verified);
    return joinFrontMatter(w.text.toUtf8(), s.body);
}

QByteArray formatProof(const Proof& p)
{
    Writer w;
    w.string("id", p.id);
    w.string("proves", p.proves);
    w.list("premises", p.premises, true);
    w.string("level", p.level);
    w.date("created", p.created);
    w.date("verified", p.verified);
    return joinFrontMatter(w.text.toUtf8(), p.body);
}

QByteArray formatVerboseProof(const VerboseProof& v)
{
    Writer w;
    w.string("proof", v.proof);
    w.string("fingerprint", v.fingerprint);
    w.date("created", v.created);
    return joinFrontMatter(w.text.toUtf8(), v.body);
}

QByteArray bodyFromText(const QByteArray& text)
{
    QByteArray body = crlfToLf(text);
    if (body.startsWith("\xEF\xBB\xBF"))
        body.remove(0, 3);
    if (!body.endsWith('\n'))
        body += '\n';
    return body;
}

} // namespace loom
