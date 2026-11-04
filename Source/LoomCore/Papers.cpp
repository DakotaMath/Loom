#include "LoomCore/Papers.h"

#include "LoomCore/Checks.h"
#include "LoomCore/FieldReader.h"
#include "LoomCore/Ids.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/TomlWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <string>

namespace loom {

namespace {

constexpr auto paperFieldList =
    u"key, aliases, cite, relevance, relevance_why, tags, summary, created, verified, work, versions, reading, renumbering";
constexpr auto workFieldList = u"type, title, authors, editors";
constexpr auto readingFieldList = u"version, level, what, date, session";
constexpr auto nameFieldList = u"last, first";
constexpr auto stubFieldList =
    u"id, kind, title, gist, stated_using, paper_premises, aliases, extracted_from, extracted_by, created, locators";
constexpr auto locatorFieldList = u"at, checked_against_pdf";

/// The type of a field's value.
enum class ValueKind { String, Integer, Date, Names };

/// The type of each bibliographic field's value (Specs/m1-core.md, "Papers": "`year`,
/// `arxiv_version`, `page_count` are integers; `posted` and `date` are dates; the rest are strings").
ValueKind valueKindOf(QStringView field)
{
    if (field == u"year" || field == u"arxiv_version" || field == u"page_count")
        return ValueKind::Integer;
    if (field == u"posted" || field == u"date")
        return ValueKind::Date;
    if (field == u"authors" || field == u"editors")
        return ValueKind::Names;
    return ValueKind::String;
}

/// What a value of `kind` is, for `wrong-type`. No side effects.
QString describeKind(ValueKind kind)
{
    switch (kind) {
    case ValueKind::String: return QStringLiteral("a string");
    case ValueKind::Integer: return QStringLiteral("a whole number");
    case ValueKind::Date: return QStringLiteral("a date");
    case ValueKind::Names: return QStringLiteral("a list of names, each `{ last = \"…\", first = \"…\" }`");
    }
    return {};
}

/// The fields a published version expects, and those it may have, by the work's type
/// (Specs/m1-core.md, "Papers", the `[[versions]]` table).
struct TypeFields {
    QStringList expected;
    QStringList optional;
};

TypeFields publishedFields(const QString& type)
{
    if (type == u"article")
        return {{"journal", "volume", "number", "pages", "year", "doi"}, {"journal_abbrev", "month", "publisher", "issn"}};
    if (type == u"book")
        return {{"publisher", "address", "year", "edition", "series", "series_volume", "isbn", "doi"}, {}};
    if (type == u"incollection" || type == u"inproceedings")
        return {{"booktitle", "publisher", "address", "pages", "year", "series", "volume", "doi"}, {}};
    if (type == u"phdthesis" || type == u"mastersthesis")
        return {{"school", "address", "year"}, {}};
    if (type == u"techreport")
        return {{"institution", "number", "year"}, {}};
    if (type == u"talk" || type == u"lecture-notes" || type == u"other")
        return {{"howpublished", "venue", "date", "url"}, {}};
    return {};
}

const QStringList arxivFields = {"arxiv_id", "arxiv_version", "posted", "primary_class", "page_count"};
const QStringList everyVersionFields = {"mr", "zbl", "url", "page_count"};

/// Whether `name` is a bibliographic field of any version of any type, so a field in the wrong
/// version is `field-not-for-type` rather than `unknown-field`.
bool isBibliographicField(const QString& name)
{
    if (arxivFields.contains(name) || everyVersionFields.contains(name))
        return true;
    for (const QString& type : workTypes) {
        const TypeFields fields = publishedFields(type);
        if (fields.expected.contains(name) || fields.optional.contains(name))
            return true;
    }
    return false;
}

/// "an article", "a book", … for `field-not-for-type`.
QString typeSingular(const QString& type)
{
    if (type == u"article") return QStringLiteral("an article");
    if (type == u"book") return QStringLiteral("a book");
    if (type == u"incollection") return QStringLiteral("a chapter in a book (incollection)");
    if (type == u"inproceedings") return QStringLiteral("a conference paper (inproceedings)");
    if (type == u"phdthesis") return QStringLiteral("a PhD thesis");
    if (type == u"mastersthesis") return QStringLiteral("a master's thesis");
    if (type == u"techreport") return QStringLiteral("a technical report");
    if (type == u"talk") return QStringLiteral("a talk");
    if (type == u"lecture-notes") return QStringLiteral("lecture notes");
    return QStringLiteral("an entry of type other");
}

/// "articles", "books", … for `field-expected` ("{type}s usually have one").
QString typePlural(const QString& type)
{
    if (type == u"article") return QStringLiteral("articles");
    if (type == u"book") return QStringLiteral("books");
    if (type == u"incollection") return QStringLiteral("chapters in books");
    if (type == u"inproceedings") return QStringLiteral("conference papers");
    if (type == u"phdthesis") return QStringLiteral("PhD theses");
    if (type == u"mastersthesis") return QStringLiteral("master's theses");
    if (type == u"techreport") return QStringLiteral("technical reports");
    if (type == u"talk") return QStringLiteral("talks");
    if (type == u"lecture-notes") return QStringLiteral("lecture notes");
    return QStringLiteral("entries of type other");
}

/// How a version is named in messages: "published version", "arXiv version arxiv-v1", "edition ed2".
QString versionPhrase(const PaperVersion& version)
{
    if (version.kind == u"arxiv")
        return QStringLiteral("arXiv version %1").arg(version.id);
    if (version.id == u"published")
        return QStringLiteral("published version");
    return QStringLiteral("edition %1").arg(version.id);
}

/// Whether a source is a URL or a path inside the paper's `local/` (Specs/m1-core.md, "Papers",
/// "Fields": "`source` is a non-empty URL (`https://…`) or a path inside the paper's `local/`").
bool isLocalPath(const QString& path)
{
    return path.startsWith(u"local/") && isProjectPath(path) && path.size() > 6;
}

/// Reads a paper entry, table by table.
struct PaperReader {
    FieldReader& r;
    Paper& paper;
    /// The paper's key or folder name, for messages.
    QString name;

    FieldChecker& checker() { return r.checker; }

    /// Reports a path that should be inside the paper's `local/` (`pdf-path`).
    void pdfPath(const QString& path, int line)
    {
        checker().add("pdf-path", line, QStringLiteral("`%1` must be inside this paper's `local/` folder.").arg(path));
    }

    /// Reads a list of names, `[{ last = "…", first = "…" }, …]`.
    std::optional<std::vector<PersonName>> names(const toml::node& node, const QString& field)
    {
        const toml::array* array = node.as_array();
        if (!array) {
            checker().wrongType(field, describeKind(ValueKind::Names), node);
            return std::nullopt;
        }
        std::vector<PersonName> out;
        for (const toml::node& element : *array) {
            const toml::table* table = element.as_table();
            if (!table) {
                checker().add("wrong-type", checker().line(element),
                    QStringLiteral("`%1` should be %2, not a list holding %3.")
                        .arg(field, describeKind(ValueKind::Names), describeToml(element)));
                r.lost = true;
                continue;
            }
            const std::size_t before = checker().problems.size();
            checker().unknownFields(*table, {"last", "first"}, field + u'.', QStringLiteral("a name"), nameFieldList);
            r.lost = r.lost || checker().problems.size() != before;
            const int line = checker().line(element);
            const std::optional<QString> last =
                checker().stringField(*table, "last", field + QStringLiteral(".last"), line, QStringLiteral("name"), Need::RequiredNonEmpty);
            const std::optional<QString> first =
                checker().stringField(*table, "first", field + QStringLiteral(".first"), line, QStringLiteral("name"), Need::Optional);
            if (!last || (table->contains("first") && !first)) {
                r.lost = true;
                continue;
            }
            out.push_back({*last, first.value_or(QString())});
        }
        return out;
    }

    /// Reads a bibliographic field (Specs/m1-core.md, "Papers", "Fields").
    /// @param field The name in messages, e.g. `published.journal`.
    std::optional<PaperField> field(const toml::node& node, const QString& field, ValueKind kind)
    {
        const int line = checker().line(node);
        const QString shape = QStringLiteral("`%1` must be `{ value = …, source = \"…\", verified = … }`.").arg(field);
        const toml::table* table = node.as_table();
        if (!table || !table->contains("value")) {
            checker().add("field-shape", line, shape);
            r.lost = true;
            return std::nullopt;
        }
        for (auto&& [key, value] : *table) {
            if (key.str() != "value" && key.str() != "source" && key.str() != "verified") {
                checker().add("field-shape", line, shape);
                r.lost = true;
                return std::nullopt;
            }
        }

        PaperField out;
        out.line = line;
        const toml::node& value = *table->get("value");
        switch (kind) {
        case ValueKind::String:
            if (const toml::value<std::string>* text = value.as_string())
                out.value = fromUtf8(text->get());
            else
                return wrong(field, kind, value);
            break;
        case ValueKind::Integer:
            if (const toml::value<int64_t>* number = value.as_integer())
                out.value = qint64(number->get());
            else
                return wrong(field, kind, value);
            break;
        case ValueKind::Date:
            if (const toml::value<toml::date>* date = value.as_date())
                out.value = QDate(date->get().year, date->get().month, date->get().day);
            else
                return wrong(field, kind, value);
            break;
        case ValueKind::Names: {
            std::optional<std::vector<PersonName>> list = names(value, field);
            if (!list) {
                r.lost = true;
                return std::nullopt;
            }
            out.value = std::move(*list);
            break;
        }
        }

        const toml::node* source = table->get("source");
        if (source && !source->is_string()) {
            checker().add("field-shape", line, shape);
            r.lost = true;
            return std::nullopt;
        }
        out.source = source ? fromUtf8(source->as_string()->get()) : QString();
        if (out.source.trimmed().isEmpty())
            checker().add("field-source", line, QStringLiteral("`%1` has no source. Every field says where it came from.").arg(field));
        else if (!out.source.startsWith(u"https://") && !isLocalPath(out.source))
            pdfPath(out.source, line);

        const toml::node* verified = table->get("verified");
        if (verified && verified->as_date()) {
            const toml::date date = verified->as_date()->get();
            out.verified = QDate(date.year, date.month, date.day);
        } else if (!verified || !verified->as_boolean() || verified->as_boolean()->get()) {
            checker().add("field-verified", line, QStringLiteral("`%1`'s `verified` must be a date or `false`.").arg(field));
            r.lost = r.lost || verified;
        }
        return out;
    }

    /// Reports a field's value of the wrong type; the field is dropped.
    std::optional<PaperField> wrong(const QString& field, ValueKind kind, const toml::node& value)
    {
        checker().wrongType(field + QStringLiteral(".value"), describeKind(kind), value);
        r.lost = true;
        return std::nullopt;
    }

    /// Reads `[work]`, and checks there are authors (or, for a book, editors).
    void work(const toml::table& root)
    {
        const toml::node* node = root.get("work");
        if (!node) {
            r.checker.missingField(QStringLiteral("work"), 1, QStringLiteral("paper"));
            return;
        }
        const toml::table* table = node->as_table();
        if (!table) {
            checker().wrongType(QStringLiteral("work"), QStringLiteral("a table, `[work]`"), *node);
            r.lost = true;
            return;
        }
        PaperWork& w = paper.work;
        w.line = checker().line(*table);
        const std::size_t before = checker().problems.size();
        checker().unknownFields(*table, {"type", "title", "authors", "editors"}, QStringLiteral("work."), QStringLiteral("`[work]`"), workFieldList);
        r.lost = r.lost || checker().problems.size() != before;

        const auto read = [&](const char* key, ValueKind kind, bool required) -> std::optional<PaperField> {
            const QString field = QStringLiteral("work.") + QString::fromLatin1(key);
            const toml::node* value = table->get(key);
            if (!value) {
                if (required)
                    checker().missingField(field, w.line, QStringLiteral("paper"));
                return std::nullopt;
            }
            return this->field(*value, field, kind);
        };
        w.type = read("type", ValueKind::String, true);
        w.title = read("title", ValueKind::String, true);
        w.authors = read("authors", ValueKind::Names, false);
        w.editors = read("editors", ValueKind::Names, false);

        if (w.type && !workTypes.contains(w.typeName()))
            checker().add("bad-value", w.type->line,
                QStringLiteral("`work.type` can't be `%1`; it must be one of %2.").arg(w.typeName(), quotedList(workTypes)));
        if (w.title && w.title->text().trimmed().isEmpty())
            checker().add("wrong-type", w.title->line, QStringLiteral("`work.title` should be a non-empty string, not an empty string."));

        // At least one author, or for a book with no authors, at least one editor.
        // A list Loom couldn't read was reported already.
        const bool hasAuthors = w.authors && !w.authors->names().empty();
        const bool hasEditors = w.editors && !w.editors->names().empty();
        const bool unreadable = (table->contains("authors") && !w.authors) || (table->contains("editors") && !w.editors);
        if (!hasAuthors && (w.typeName() != u"book" || !hasEditors) && !unreadable) {
            const QString message = w.typeName() == u"book"
                ? QStringLiteral("%1 has no authors (and no editors).").arg(name)
                : QStringLiteral("%1 has no authors.").arg(name);
            checker().add("authors-missing", w.authors ? w.authors->line : w.line, message);
        }
    }

    /// Reads one `[[versions]]` table: its ID and kind, `pdf`, `differences`, and its fields.
    /// @param ids The version IDs seen so far, for `version-duplicate`.
    void version(const toml::table& table, QSet<QString>& ids)
    {
        PaperVersion v;
        v.line = checker().line(table);
        FieldReader local(checker().file, QStringLiteral("version"), 0);
        v.id = local.string(table, "id", Need::Required, v.line).value_or(QString());
        v.kind = local.string(table, "kind", Need::Required, v.line).value_or(QString());
        v.pdf = local.string(table, "pdf", Need::Optional).value_or(QString());
        v.differences = local.string(table, "differences", Need::Optional).value_or(QString());
        r.lost = r.lost || local.lost;
        checker().problems.insert(checker().problems.end(), local.checker.problems.begin(), local.checker.problems.end());

        const QString type = paper.work.typeName();
        const int idLine = local.lines.of(QStringLiteral("id"));
        if (!v.kind.isEmpty() && v.kind != u"arxiv" && v.kind != u"published")
            checker().add("bad-value", local.lines.of(QStringLiteral("kind")),
                QStringLiteral("`kind` can't be `%1`; it must be one of `arxiv`, `published`.").arg(v.kind));
        static const QRegularExpression arxivId(QStringLiteral("^arxiv-v([1-9][0-9]*)$"));
        static const QRegularExpression editionId(QStringLiteral("^ed([2-9]|[1-9][0-9]+)$"));
        if (!v.id.isEmpty()) {
            if (v.kind == u"arxiv" && !arxivId.match(v.id).hasMatch())
                checker().add("version-id", idLine,
                    QStringLiteral("`%1` isn't a valid version id for an arxiv version: use arxiv-v<N>.").arg(v.id));
            else if (v.kind == u"published" && v.id != u"published" && !(type == u"book" && editionId.match(v.id).hasMatch()))
                checker().add("version-id", idLine,
                    QStringLiteral("`%1` isn't a valid version id for a published version: use %2.")
                        .arg(v.id, type == u"book" ? QStringLiteral("published or ed<N>") : QStringLiteral("published")));
            if (ids.contains(v.id))
                checker().add("version-duplicate", idLine, QStringLiteral("Two versions are called `%1`.").arg(v.id));
            ids.insert(v.id);
        }
        if (!v.pdf.isEmpty() && !isLocalPath(v.pdf))
            pdfPath(v.pdf, local.lines.of(QStringLiteral("pdf")));

        // Every other key is a bibliographic field; in toml++'s order, which is by name.
        const bool knownKind = v.kind == u"arxiv" || v.kind == u"published";
        const bool knownType = workTypes.contains(type);
        const QStringList allowed = versionFieldOrder(v.kind, type);
        for (auto&& [key, node] : table) {
            const QString fieldName = fromUtf8(key.str());
            if (fieldName == u"id" || fieldName == u"kind" || fieldName == u"pdf" || fieldName == u"differences")
                continue;
            const QString label = (v.id.isEmpty() ? QStringLiteral("versions") : v.id) + u'.' + fieldName;
            if (!isBibliographicField(fieldName)) {
                checker().add("unknown-field", checker().line(key),
                    QStringLiteral("`%1` isn't a field of a version. Is it misspelt? The fields are: id, kind, pdf, differences, "
                                   "and the bibliographic fields of its kind and the work's type.").arg(label));
                r.lost = true;
                continue;
            }
            // With an unknown kind or type, which fields belong can't be said: keep them all.
            if (knownKind && knownType && !allowed.contains(fieldName)) {
                checker().add("field-not-for-type", checker().line(key),
                    QStringLiteral("`%1` isn't a field of %2.")
                        .arg(label, v.kind == u"arxiv" ? QStringLiteral("an arXiv version")
                                                      : typeSingular(type) + QStringLiteral("'s published version")));
                r.lost = true;
                continue;
            }
            if (std::optional<PaperField> value = field(node, label, valueKindOf(fieldName)))
                v.fields.push_back({fieldName, std::move(*value)});
        }
        std::stable_sort(v.fields.begin(), v.fields.end(), [](const NamedField& a, const NamedField& b) { return a.field.line < b.field.line; });

        if (v.kind == u"arxiv") {
            const QRegularExpressionMatch match = arxivId.match(v.id);
            const PaperField* number = v.field(u"arxiv_version");
            if (match.hasMatch() && number && std::holds_alternative<qint64>(number->value)
                && std::get<qint64>(number->value) != match.captured(1).toLongLong())
                checker().add("arxiv-version-mismatch", number->line,
                    QStringLiteral("Version `%1` has `arxiv_version = %2`; they must agree.").arg(v.id, number->text()));
        }
        if (knownKind && (knownType || v.kind == u"arxiv")) {
            for (const QString& expected : expectedVersionFields(v.kind, type)) {
                if (!v.field(expected))
                    checker().add("field-expected", v.line,
                        QStringLiteral("%1's %2 has no `%3`; %4 usually have one.")
                            .arg(name, versionPhrase(v), expected,
                                 v.kind == u"arxiv" ? QStringLiteral("arXiv versions") : typePlural(type)),
                        Severity::Warning);
            }
        }
        paper.versions.push_back(std::move(v));
    }

    /// Reads `[[versions]]`: at least one is needed.
    void versions(const toml::table& root)
    {
        const toml::node* node = root.get("versions");
        const toml::array* array = node ? node->as_array() : nullptr;
        if (node && (!array || (!array->empty() && !array->is_array_of_tables()))) {
            checker().wrongType(QStringLiteral("versions"), QStringLiteral("a list of versions, each `[[versions]]`"), *node);
            r.lost = true;
            return;
        }
        if (!array || array->empty()) {
            checker().missingField(QStringLiteral("versions"), 1, QStringLiteral("paper"));
            return;
        }
        QSet<QString> ids;
        for (const toml::node& element : *array)
            version(*element.as_table(), ids);
    }

    /// Reads `[[reading]]`.
    void readings(const toml::table& root)
    {
        const toml::node* node = root.get("reading");
        if (!node)
            return;
        const toml::array* array = node->as_array();
        if (!array || (!array->empty() && !array->is_array_of_tables())) {
            checker().wrongType(QStringLiteral("reading"), QStringLiteral("a list of readings, each `[[reading]]`"), *node);
            r.lost = true;
            return;
        }
        static const QRegularExpression sessionRule(QStringLiteral("^Sessions/[0-9]{4}-[0-9]{2}-[0-9]{2}$"));
        for (const toml::node& element : *array) {
            const toml::table& table = *element.as_table();
            ReadingEntry e;
            e.line = checker().line(table);
            FieldReader local(checker().file, QStringLiteral("reading"), 0);
            local.unknownFields(table, {"version", "level", "what", "date", "session"}, readingFieldList);
            e.version = local.string(table, "version", Need::Required, e.line).value_or(QString());
            e.level = local.string(table, "level", Need::Required, e.line).value_or(QString());
            e.what = local.string(table, "what", Need::RequiredNonEmpty, e.line).value_or(QString());
            e.date = local.date(table, "date", true, e.line);
            e.session = local.string(table, "session", Need::Optional).value_or(QString());
            if (!e.level.isEmpty() && !readingLevels.contains(e.level))
                local.badValue(QStringLiteral("level"), e.level, quotedList(readingLevels));
            if (!e.session.isEmpty() && !sessionRule.match(e.session).hasMatch())
                local.badValue(QStringLiteral("session"), e.session, QStringLiteral("`Sessions/<YYYY-MM-DD>`"));
            r.lost = r.lost || local.lost;
            checker().problems.insert(checker().problems.end(), local.checker.problems.begin(), local.checker.problems.end());
            e.lines = std::move(local.lines);
            paper.readings.push_back(std::move(e));
        }
    }

    /// Reads `[[renumbering]]`: two or more labels and `checked`.
    void renumberings(const toml::table& root)
    {
        const toml::node* node = root.get("renumbering");
        if (!node)
            return;
        const toml::array* array = node->as_array();
        if (!array || (!array->empty() && !array->is_array_of_tables())) {
            checker().wrongType(QStringLiteral("renumbering"), QStringLiteral("a list of renumberings, each `[[renumbering]]`"), *node);
            r.lost = true;
            return;
        }
        for (const toml::node& element : *array) {
            const toml::table& table = *element.as_table();
            Renumbering e;
            e.line = checker().line(table);
            std::vector<std::pair<int, std::pair<QString, QString>>> labels;
            for (auto&& [key, value] : table) {
                if (key.str() == "checked")
                    continue;
                const QString version = fromUtf8(key.str());
                if (!value.is_string()) {
                    checker().wrongType(QStringLiteral("renumbering.") + version, QStringLiteral("a string: the item's label in that version"), value);
                    r.lost = true;
                    continue;
                }
                labels.push_back({checker().line(key), {version, fromUtf8(value.as_string()->get())}});
            }
            std::stable_sort(labels.begin(), labels.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (auto& [line, label] : labels) {
                e.labels.push_back(std::move(label));
                e.labelLines << line;
            }
            if (e.labels.size() < 2)
                checker().add("bad-value", e.line,
                    QStringLiteral("A `[[renumbering]]` entry needs the item's label in two or more versions."));

            const toml::node* checked = table.get("checked");
            if (!checked) {
                checker().missingField(QStringLiteral("renumbering.checked"), e.line, QStringLiteral("renumbering"));
            } else if (const toml::value<toml::date>* date = checked->as_date()) {
                e.checked = QDate(date->get().year, date->get().month, date->get().day);
            } else if (!checked->as_boolean() || checked->as_boolean()->get()) {
                checker().wrongType(QStringLiteral("renumbering.checked"), QStringLiteral("a date or `false`"), *checked);
                r.lost = true;
            }
            paper.renumberings.push_back(std::move(e));
        }
    }

    /// `cite`, readings and renumberings must name versions of this paper.
    void versionReferences()
    {
        if (!paper.cite.isEmpty() && !paper.version(paper.cite))
            checker().add("cite-unknown", r.lines.of(QStringLiteral("cite")),
                QStringLiteral("`cite` names `%1`, which isn't one of this paper's versions.").arg(paper.cite));
        for (const ReadingEntry& e : paper.readings) {
            if (!e.version.isEmpty() && !paper.version(e.version))
                checker().add("version-unknown", e.lines.of(QStringLiteral("version")),
                    QStringLiteral("`reading` names version `%1`, which this paper doesn't have.").arg(e.version));
        }
        for (const Renumbering& e : paper.renumberings) {
            for (qsizetype i = 0; i < qsizetype(e.labels.size()); ++i) {
                if (!paper.version(e.labels[i].first))
                    checker().add("version-unknown", e.labelLines.value(i, e.line),
                        QStringLiteral("`renumbering` names version `%1`, which this paper doesn't have.").arg(e.labels[i].first));
            }
        }
    }
};

/// The name of the folder holding `path`'s folder, e.g. `SmithJones24` for
/// `Papers/SmithJones24/paper.toml`, or for a stub `Papers/SmithJones24/Statements/7RT2.toml`
/// with `levels` 2.
QString folderName(const QString& path, int levels)
{
    QString folder = QFileInfo(path).path();
    for (int i = 1; i < levels; ++i)
        folder = QFileInfo(folder).path();
    return QFileInfo(folder).fileName();
}

/// Writes TOML lines in order.
struct Writer {
    QString text;

    /// `key = value`, as written.
    void line(const QString& key, const QString& value) { text += key + QStringLiteral(" = ") + value + u'\n'; }
    /// A string, multi-line if it has line breaks, so a summary reads as it was written.
    void string(const char* key, const QString& value, bool always = false)
    {
        if (value.isEmpty() && !always)
            return;
        line(QString::fromLatin1(key), value.contains(u'\n') ? tomlMultilineStringAsIs(value) : tomlBasicString(value));
    }
    /// A list of strings, left out when empty.
    void list(const char* key, const QStringList& values)
    {
        if (!values.isEmpty())
            line(QString::fromLatin1(key), tomlStringArray(values));
    }
    /// A date, left out when invalid.
    void date(const char* key, const QDate& value)
    {
        if (value.isValid())
            line(QString::fromLatin1(key), tomlDate(value));
    }
};

/// A date, or `false` for an invalid one (a `verified` or `checked` not yet set). No side effects.
QString dateOrFalse(const QDate& date)
{
    return date.isValid() ? tomlDate(date) : QStringLiteral("false");
}

/// A field's value as TOML; names as `[{ last = "…", first = "…" }, …]`. No side effects.
QString formatValue(const FieldValue& value)
{
    if (const QString* text = std::get_if<QString>(&value))
        return tomlBasicString(*text);
    if (const qint64* number = std::get_if<qint64>(&value))
        return QString::number(*number);
    if (const QDate* date = std::get_if<QDate>(&value))
        return tomlDate(*date);
    QStringList names;
    for (const PersonName& name : std::get<std::vector<PersonName>>(value))
        names << QStringLiteral("{ last = %1, first = %2 }").arg(tomlBasicString(name.last), tomlBasicString(name.first));
    return u'[' + names.join(u", ") + u']';
}

/// A field as its inline table, on one line. No side effects.
QString formatField(const PaperField& field)
{
    return QStringLiteral("{ value = %1, source = %2, verified = %3 }")
        .arg(formatValue(field.value), tomlBasicString(field.source), dateOrFalse(field.verified));
}

} // namespace

QString PaperField::text() const
{
    if (const QString* string = std::get_if<QString>(&value))
        return *string;
    if (const qint64* number = std::get_if<qint64>(&value))
        return QString::number(*number);
    if (const QDate* date = std::get_if<QDate>(&value))
        return tomlDate(*date);
    QStringList out;
    for (const PersonName& name : std::get<std::vector<PersonName>>(value))
        out << (name.first.isEmpty() ? name.last : name.last + QStringLiteral(", ") + name.first);
    return out.join(QStringLiteral(" and "));
}

std::vector<PersonName> PaperField::names() const
{
    if (const auto* names = std::get_if<std::vector<PersonName>>(&value))
        return *names;
    return {};
}

const PaperField* PaperVersion::field(QStringView name) const
{
    const auto it = std::find_if(fields.begin(), fields.end(), [&](const NamedField& f) { return f.name == name; });
    return it == fields.end() ? nullptr : &it->field;
}

QString PaperVersion::text(QStringView name) const
{
    const PaperField* f = field(name);
    return f ? f->text() : QString();
}

QString PaperWork::typeName() const
{
    return type ? type->text() : QString();
}

const PaperVersion* Paper::version(const QString& id) const
{
    const auto it = std::find_if(versions.begin(), versions.end(), [&](const PaperVersion& v) { return v.id == id; });
    return it == versions.end() ? nullptr : &*it;
}

std::vector<PersonName> Paper::keyNames() const
{
    std::vector<PersonName> names = work.authors ? work.authors->names() : std::vector<PersonName>();
    if (names.empty() && work.editors)
        names = work.editors->names();
    return names;
}

QString Paper::readingLevel() const
{
    qsizetype highest = -1;
    for (const ReadingEntry& e : readings)
        highest = std::max(highest, readingLevels.indexOf(e.level));
    return highest >= 0 ? readingLevels[highest] : QString();
}

const Locator* Stub::locator(const QString& version) const
{
    const auto it = std::find_if(locators.begin(), locators.end(), [&](const Locator& l) { return l.version == version; });
    return it == locators.end() ? nullptr : &*it;
}

bool isCited(const Stub& stub)
{
    const Locator* locator = stub.locator(stub.extractedFrom);
    return locator && locator->checkedAgainstPdf.isValid();
}

QStringList versionFieldOrder(const QString& kind, const QString& workType)
{
    QStringList order;
    if (kind == u"arxiv") {
        order = arxivFields;
    } else if (kind == u"published") {
        const TypeFields fields = publishedFields(workType);
        order = fields.expected + fields.optional;
    }
    for (const QString& field : everyVersionFields) {
        if (!order.contains(field))
            order << field;
    }
    return order;
}

QStringList expectedVersionFields(const QString& kind, const QString& workType)
{
    if (kind == u"arxiv")
        return arxivFields;
    if (kind == u"published")
        return publishedFields(workType).expected;
    return {};
}

PaperReading readPaper(const QByteArray& bytes, const QString& path, bool keyRequired)
{
    PaperReading reading;
    FieldReader r(path, QStringLiteral("paper"), 0);
    const std::optional<toml::table> table = parsePlainToml(bytes, r.checker);
    if (!table) {
        reading.problems = std::move(r.checker.problems);
        return reading;
    }

    Paper& p = reading.document;
    r.unknownFields(*table, {"key", "aliases", "cite", "relevance", "relevance_why", "tags", "summary", "created", "verified",
                             "work", "versions", "reading", "renumbering"}, paperFieldList);
    p.key = r.string(*table, "key", keyRequired ? Need::RequiredNonEmpty : Need::Optional).value_or(QString());
    p.aliases = r.list(*table, "aliases", QStringLiteral("a list of old keys"), false).value_or(QStringList());
    p.cite = r.string(*table, "cite", keyRequired ? Need::RequiredNonEmpty : Need::Optional).value_or(QString());
    const std::optional<qint64> relevance = r.integer(*table, "relevance", true);
    p.relevance = relevance.value_or(0);
    p.relevanceWhy = r.string(*table, "relevance_why", Need::Optional).value_or(QString());
    p.tags = r.list(*table, "tags", QStringLiteral("a list of tags"), false).value_or(QStringList());
    p.summary = r.string(*table, "summary", Need::Optional).value_or(QString());
    p.created = r.date(*table, "created", keyRequired);
    p.verified = r.date(*table, "verified", false);

    if (relevance && *relevance < 1)
        r.checker.add("bad-value", r.lines.of(QStringLiteral("relevance")),
            QStringLiteral("`relevance` can't be `%1`; it must be a whole number from 1 up (1 is most relevant).").arg(*relevance));
    r.repeats(QStringLiteral("tags"), p.tags);
    r.repeats(QStringLiteral("aliases"), p.aliases);

    const QString folder = folderName(path, 1);
    PaperReader reader{r, p, p.key.isEmpty() ? folder : p.key};
    reader.work(*table);
    reader.versions(*table);
    reader.readings(*table);
    reader.renumberings(*table);
    reader.versionReferences();

    reading.parsed = true;
    reading.lossless = !r.lost;
    reading.lines = std::move(r.lines);
    reading.problems = std::move(r.checker.problems);
    sortProblems(reading.problems);
    return reading;
}

StubReading readStub(const QByteArray& bytes, const QString& path)
{
    StubReading reading;
    FieldReader r(path, QStringLiteral("stub"), 0);
    const std::optional<toml::table> table = parsePlainToml(bytes, r.checker);
    if (!table) {
        reading.problems = std::move(r.checker.problems);
        return reading;
    }

    Stub& s = reading.document;
    const QString name = QFileInfo(path).completeBaseName();
    r.unknownFields(*table, {"id", "kind", "title", "gist", "stated_using", "paper_premises", "aliases", "extracted_from",
                             "extracted_by", "created", "locators"}, stubFieldList);
    s.id = r.string(*table, "id", Need::Required).value_or(QString());
    s.kind = r.string(*table, "kind", Need::Required).value_or(QString());
    s.title = r.string(*table, "title", Need::Optional).value_or(QString());
    const toml::node* gistNode = table->get("gist");
    if (gistNode && gistNode->is_string()) {
        s.gist = fromUtf8(gistNode->as_string()->get());
        r.lines.fields.insert(QStringLiteral("gist"), r.checker.line(*gistNode));
    } else if (gistNode) {
        r.checker.wrongType(QStringLiteral("gist"), QStringLiteral("a string"), *gistNode);
        r.lost = true;
    }
    s.statedUsing = r.list(*table, "stated_using", QStringLiteral("a list of tags"), false).value_or(QStringList());
    s.paperPremises = r.list(*table, "paper_premises", QStringLiteral("a list of tags"), false).value_or(QStringList());
    s.aliases = r.list(*table, "aliases", QStringLiteral("a list of aliases"), false).value_or(QStringList());
    s.extractedFrom = r.string(*table, "extracted_from", Need::RequiredNonEmpty).value_or(QString());
    s.extractedBy = r.string(*table, "extracted_by", Need::Required).value_or(QString());
    s.created = r.date(*table, "created", true);

    const QString id = s.id.isEmpty() ? name : s.id;
    if (!s.id.isEmpty()) {
        if (!isTag(s.id))
            r.tagSyntax(s.id, r.lines.of(QStringLiteral("id")));
        else if (s.id != name)
            r.checker.add("id-mismatch", r.lines.of(QStringLiteral("id")),
                QStringLiteral("The id is `%1` but the file is named `%2`; they must match.").arg(s.id, name));
    }
    if (!s.kind.isEmpty() && !statementKinds.contains(s.kind))
        r.badValue(QStringLiteral("kind"), s.kind, quotedList(statementKinds));
    if (s.title.contains(u'\n') || s.title.contains(u'\r'))
        r.checker.add("wrong-type", r.lines.of(QStringLiteral("title")),
            QStringLiteral("`title` should be a string on one line, not a string with line breaks."));
    if (s.gist.trimmed().isEmpty() && !(gistNode && !gistNode->is_string()))
        r.checker.add("stub-gist-empty", r.lines.of(QStringLiteral("gist")), QStringLiteral("Stub %1 has no gist.").arg(id));
    if (!s.extractedBy.isEmpty() && !isCheckAuthor(s.extractedBy))
        r.badValue(QStringLiteral("extracted_by"), s.extractedBy, QStringLiteral("`dakota`, `agent:<model>`"));
    r.repeats(QStringLiteral("stated_using"), s.statedUsing);
    r.repeats(QStringLiteral("paper_premises"), s.paperPremises);
    r.repeats(QStringLiteral("aliases"), s.aliases);
    r.aliases(s.aliases);

    const toml::node* locatorsNode = table->get("locators");
    if (!locatorsNode) {
        r.checker.missingField(QStringLiteral("locators"), 1, QStringLiteral("stub"));
    } else if (const toml::table* locators = locatorsNode->as_table()) {
        for (auto&& [key, node] : *locators) {
            const QString version = fromUtf8(key.str());
            const QString field = QStringLiteral("locators.") + version;
            const int line = r.checker.line(key);
            const toml::table* entry = node.as_table();
            if (!entry) {
                r.checker.wrongType(field, QStringLiteral("`{ at = \"…\", checked_against_pdf = … }`"), node);
                r.lost = true;
                continue;
            }
            const std::size_t before = r.checker.problems.size();
            r.checker.unknownFields(*entry, {"at", "checked_against_pdf"}, field + u'.', QStringLiteral("a locator"), locatorFieldList);
            r.lost = r.lost || r.checker.problems.size() != before;
            Locator locator;
            locator.version = version;
            locator.line = line;
            const toml::node* at = entry->get("at");
            if (!at || (at->is_string() && at->as_string()->get().empty()))
                r.checker.add("missing-field", line, QStringLiteral("`%1.at` is missing; every locator says where the result is.").arg(field));
            else if (!at->is_string()) {
                r.checker.wrongType(field + QStringLiteral(".at"), QStringLiteral("a string"), *at);
                r.lost = true;
            } else {
                locator.at = fromUtf8(at->as_string()->get());
            }
            const toml::node* checked = entry->get("checked_against_pdf");
            if (!checked) {
                r.checker.missingField(field + QStringLiteral(".checked_against_pdf"), line, QStringLiteral("locator"));
            } else if (const toml::value<toml::date>* date = checked->as_date()) {
                locator.checkedAgainstPdf = QDate(date->get().year, date->get().month, date->get().day);
            } else if (!checked->as_boolean() || checked->as_boolean()->get()) {
                r.checker.wrongType(field + QStringLiteral(".checked_against_pdf"), QStringLiteral("a date or `false`"), *checked);
                r.lost = true;
            }
            s.locators.push_back(locator);
        }
        std::stable_sort(s.locators.begin(), s.locators.end(), [](const Locator& a, const Locator& b) { return a.line < b.line; });
        if (!s.extractedFrom.isEmpty() && !s.locator(s.extractedFrom))
            r.checker.add("stub-locator-missing", r.lines.of(QStringLiteral("extracted_from")),
                QStringLiteral("Stub %1 was extracted from `%2` but has no locator for it.").arg(id, s.extractedFrom));
    } else {
        r.checker.wrongType(QStringLiteral("locators"), QStringLiteral("a table, `[locators]`"), *locatorsNode);
        r.lost = true;
    }

    reading.parsed = true;
    reading.lossless = !r.lost;
    reading.lines = std::move(r.lines);
    reading.problems = std::move(r.checker.problems);
    sortProblems(reading.problems);
    return reading;
}

QByteArray formatPaper(const Paper& p)
{
    Writer w;
    w.string("key", p.key);
    w.list("aliases", p.aliases);
    w.string("cite", p.cite);
    if (p.relevance != 0)
        w.line(QStringLiteral("relevance"), QString::number(p.relevance));
    w.string("relevance_why", p.relevanceWhy);
    w.list("tags", p.tags);
    w.string("summary", p.summary);
    w.date("created", p.created);
    w.date("verified", p.verified);

    w.text += QStringLiteral("\n[work]\n");
    const auto workField = [&](const char* key, const std::optional<PaperField>& field) {
        if (field)
            w.line(QString::fromLatin1(key), formatField(*field));
    };
    workField("type", p.work.type);
    workField("title", p.work.title);
    workField("authors", p.work.authors);
    workField("editors", p.work.editors);

    const QString type = p.work.typeName();
    for (const PaperVersion& v : p.versions) {
        w.text += QStringLiteral("\n[[versions]]\n");
        w.string("id", v.id, true);
        w.string("kind", v.kind, true);
        // The spec's order, then any field it doesn't place (kept, never dropped, when the kind
        // or type is unknown), in the order read.
        const QStringList order = versionFieldOrder(v.kind, type);
        std::vector<const NamedField*> fields;
        for (const QString& name : order) {
            for (const NamedField& f : v.fields) {
                if (f.name == name)
                    fields.push_back(&f);
            }
        }
        for (const NamedField& f : v.fields) {
            if (!order.contains(f.name))
                fields.push_back(&f);
        }
        for (const NamedField* f : fields)
            w.line(f->name, formatField(f->field));
        w.string("pdf", v.pdf);
        w.string("differences", v.differences);
    }
    for (const ReadingEntry& e : p.readings) {
        w.text += QStringLiteral("\n[[reading]]\n");
        w.string("version", e.version, true);
        w.string("level", e.level, true);
        w.string("what", e.what, true);
        w.date("date", e.date);
        w.string("session", e.session);
    }
    for (const Renumbering& e : p.renumberings) {
        w.text += QStringLiteral("\n[[renumbering]]\n");
        for (const auto& [version, label] : e.labels)
            w.line(tomlKey(version), tomlBasicString(label));
        w.line(QStringLiteral("checked"), dateOrFalse(e.checked));
    }
    return w.text.toUtf8();
}

QByteArray formatStub(const Stub& s)
{
    Writer w;
    w.string("id", s.id);
    w.string("kind", s.kind);
    w.string("title", s.title);
    w.string("gist", s.gist, true);
    w.list("stated_using", s.statedUsing);
    w.list("paper_premises", s.paperPremises);
    w.list("aliases", s.aliases);
    w.string("extracted_from", s.extractedFrom);
    w.string("extracted_by", s.extractedBy);
    w.date("created", s.created);
    w.text += QStringLiteral("\n[locators]\n");
    for (const Locator& l : s.locators)
        w.line(tomlKey(l.version), QStringLiteral("{ at = %1, checked_against_pdf = %2 }")
                                       .arg(tomlBasicString(l.at), dateOrFalse(l.checkedAgainstPdf)));
    return w.text.toUtf8();
}

} // namespace loom
