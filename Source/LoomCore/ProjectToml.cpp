#include "LoomCore/ProjectToml.h"

#include "LoomCore/ProjectFiles.h"
#include "LoomCore/TomlWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace loom {

namespace {

/// The fields of `loom.toml`, as the `unknown-field` message lists them.
constexpr auto projectFieldList = u"format, name, idea, created, tex.preamble, tags";
/// The fields of a tag, likewise.
constexpr auto tagFieldList = u"id, kind, title";

QString fromUtf8(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

/// The line a value or table starts on; 1 if toml++ doesn't know (Specs/m1-core.md, "Validation":
/// "the line of the offending value, else of its table header, else 1").
int lineOf(const toml::source_region& region)
{
    return region.begin.line > 0 ? static_cast<int>(region.begin.line) : 1;
}

/// What a TOML value is, for the `wrong-type` message ("…, not {what it is}").
QString describe(const toml::node& node)
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

/// Whether a string field may be absent or empty.
enum class Need { Optional, Required, RequiredNonEmpty };

/// Collects the problems found in `loom.toml`, with the spec's messages.
struct Checker {
    std::vector<Problem> problems;

    void add(const char* code, int line, const QString& message)
    {
        problems.push_back({Severity::Error, QString::fromLatin1(code), QStringLiteral("loom.toml"), line, message});
    }

    void unknownFields(const toml::table& table, std::initializer_list<std::string_view> known,
                       const QString& prefix, const QString& thing, QStringView fieldList)
    {
        for (auto&& [key, node] : table) {
            if (std::find(known.begin(), known.end(), key.str()) != known.end())
                continue;
            add("unknown-field", lineOf(key.source()),
                QStringLiteral("`%1%2` isn't a field of %3. Is it misspelt? The fields are: %4.")
                    .arg(prefix, fromUtf8(key.str()), thing, fieldList));
        }
    }

    void missingField(const QString& field, int line, const QString& thing)
    {
        add("missing-field", line, QStringLiteral("`%1` is missing; every %2 needs one.").arg(field, thing));
    }

    void wrongType(const QString& field, const QString& expected, const toml::node& node)
    {
        add("wrong-type", lineOf(node.source()),
            QStringLiteral("`%1` should be %2, not %3.").arg(field, expected, describe(node)));
    }

    /// Reads a string field of `table`, reporting it if it is missing, not a string, or empty
    /// when it mustn't be.
    /// @returns The value, or nothing if absent or wrong.
    std::optional<QString> stringField(const toml::table& table, std::string_view key, const QString& field,
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
};

/// Reads `tex.preamble`: a list of project paths to existing `.tex` files.
void readPreamble(Checker& checker, const toml::table& root, const QString& projectRoot, ProjectConfig& config)
{
    const toml::node* texNode = root.get("tex");
    if (!texNode) {
        checker.missingField(QStringLiteral("tex.preamble"), 1, QStringLiteral("project"));
        return;
    }
    const toml::table* tex = texNode->as_table();
    if (!tex) {
        checker.wrongType(QStringLiteral("tex"), QStringLiteral("a table"), *texNode);
        return;
    }
    checker.unknownFields(*tex, {"preamble"}, QStringLiteral("tex."), QStringLiteral("loom.toml"), projectFieldList);

    const toml::node* preambleNode = tex->get("preamble");
    if (!preambleNode) {
        checker.missingField(QStringLiteral("tex.preamble"), lineOf(tex->source()), QStringLiteral("project"));
        return;
    }
    const toml::array* preamble = preambleNode->as_array();
    if (!preamble) {
        checker.wrongType(QStringLiteral("tex.preamble"), QStringLiteral("a list of paths"), *preambleNode);
        return;
    }
    for (const toml::node& entry : *preamble) {
        const int line = lineOf(entry.source());
        const toml::value<std::string>* value = entry.as_string();
        if (!value) {
            checker.add("wrong-type", line,
                QStringLiteral("`tex.preamble` should be a list of paths, not a list holding %1.").arg(describe(entry)));
            continue;
        }
        const QString path = fromUtf8(value->get());
        const QString badPath = QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.").arg(path);
        if (!isProjectPath(path)) {
            checker.add("bad-path", line, badPath);
            continue;
        }
        if (!path.endsWith(u".tex")) {
            checker.add("bad-value", line,
                QStringLiteral("`tex.preamble` can't be `%1`; it must be one of the project's `.tex` files.").arg(path));
            continue;
        }
        const std::optional<QString> resolved = resolveProjectPath(projectRoot, path);
        if (!resolved) {
            checker.add("bad-path", line, badPath);
            continue;
        }
        if (!QFileInfo(*resolved).isFile()) {
            checker.add("preamble-missing", line,
                QStringLiteral("The preamble file `%1` listed in loom.toml doesn't exist.").arg(path));
            continue;
        }
        config.preamble << path;
    }
}

/// Reads `[[tags]]`: each tag's fields, the ID's syntax, and that no ID is declared twice.
void readTags(Checker& checker, const toml::table& root, ProjectConfig& config)
{
    const toml::node* tagsNode = root.get("tags");
    if (!tagsNode)
        return;
    const toml::array* tags = tagsNode->as_array();
    if (!tags) {
        checker.wrongType(QStringLiteral("tags"), QStringLiteral("a list of tables"), *tagsNode);
        return;
    }

    static const QRegularExpression tagSyntax(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    const QString thing = QStringLiteral("tag");
    QSet<QString> seen;
    for (const toml::node& entry : *tags) {
        const toml::table* tag = entry.as_table();
        if (!tag) {
            checker.add("wrong-type", lineOf(entry.source()),
                QStringLiteral("`tags` should be a list of tables, not a list holding %1.").arg(describe(entry)));
            continue;
        }
        const int line = lineOf(tag->source());
        checker.unknownFields(*tag, {"id", "kind", "title"}, QString(), QStringLiteral("a tag"), tagFieldList);

        const std::optional<QString> id = checker.stringField(*tag, "id", QStringLiteral("id"), line, thing, Need::Required);
        const std::optional<QString> kind = checker.stringField(*tag, "kind", QStringLiteral("kind"), line, thing, Need::Required);
        const std::optional<QString> title = checker.stringField(*tag, "title", QStringLiteral("title"), line, thing, Need::RequiredNonEmpty);

        bool good = id && kind && title;
        if (id) {
            const int idLine = lineOf(tag->get("id")->source());
            if (!tagSyntax.match(*id).hasMatch()) {
                checker.add("tag-syntax", idLine,
                    QStringLiteral("`%1` isn't a valid tag name: use lowercase letters, digits and single hyphens, like `energy-estimates`.").arg(*id));
                good = false;
            } else if (seen.contains(*id)) {
                checker.add("tag-duplicate", idLine, QStringLiteral("The tag `%1` is declared twice in loom.toml.").arg(*id));
                good = false;
            }
            seen.insert(*id);
        }
        if (kind && *kind != u"equation" && *kind != u"technique" && *kind != u"other") {
            checker.add("bad-value", lineOf(tag->get("kind")->source()),
                QStringLiteral("`kind` can't be `%1`; it must be one of `equation`, `technique`, `other`.").arg(*kind));
            good = false;
        }
        if (good)
            config.tags.push_back({*id, *kind, *title});
    }
}

} // namespace

ProjectTomlReading readProjectToml(const QByteArray& bytes, const QString& root)
{
    ProjectTomlReading reading;
    Checker checker;

    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), std::string_view("loom.toml"));
    } catch (const toml::parse_error& error) {
        QString description = fromUtf8(error.description());
        while (description.endsWith(u'.'))
            description.chop(1);
        checker.add("toml-syntax", lineOf(error.source()), QStringLiteral("This isn't valid TOML: %1.").arg(description));
        reading.problems = std::move(checker.problems);
        return reading;
    }

    ProjectConfig& config = reading.config;
    const QString project = QStringLiteral("project");

    // `format` first: a newer format may have fields this Loom doesn't know (Specs/m1-core.md, D7).
    if (const toml::node* node = table.get("format")) {
        if (const toml::value<int64_t>* value = node->as_integer()) {
            const qint64 format = value->get();
            if (format > projectFormat) {
                reading.formatTooNew = true;
                checker.problems.clear();
                checker.add("format-too-new", lineOf(node->source()),
                    QStringLiteral("This project was made by a newer Loom (format %1). Update Loom to open it.").arg(format));
                reading.problems = std::move(checker.problems);
                return reading;
            }
            if (format < projectFormat)
                checker.add("bad-value", lineOf(node->source()),
                    QStringLiteral("`format` can't be `%1`; it must be one of `%2`.").arg(format).arg(projectFormat));
        } else {
            checker.wrongType(QStringLiteral("format"), QStringLiteral("a whole number"), *node);
        }
    } else {
        checker.missingField(QStringLiteral("format"), 1, project);
    }

    checker.unknownFields(table, {"format", "name", "idea", "created", "tex", "tags"}, QString(),
                          QStringLiteral("loom.toml"), projectFieldList);

    if (const std::optional<QString> name = checker.stringField(table, "name", QStringLiteral("name"), 1, project, Need::RequiredNonEmpty))
        config.name = *name;
    if (const std::optional<QString> idea = checker.stringField(table, "idea", QStringLiteral("idea"), 1, project, Need::Optional))
        config.idea = *idea;

    if (const toml::node* node = table.get("created")) {
        if (const toml::value<toml::date>* value = node->as_date()) {
            const toml::date date = value->get();
            config.created = QDate(date.year, date.month, date.day);
        } else {
            checker.wrongType(QStringLiteral("created"), QStringLiteral("a date"), *node);
        }
    } else {
        checker.missingField(QStringLiteral("created"), 1, project);
    }

    readPreamble(checker, table, root, config);
    readTags(checker, table, config);

    reading.problems = std::move(checker.problems);
    sortProblems(reading.problems);
    return reading;
}

QByteArray formatProjectToml(const ProjectConfig& config)
{
    QString text;
    text += QStringLiteral("format = %1\n").arg(config.format);
    text += QStringLiteral("name = %1\n").arg(tomlBasicString(config.name));
    if (!config.idea.isEmpty())
        text += QStringLiteral("idea = %1\n").arg(tomlMultilineString(config.idea));
    text += QStringLiteral("created = %1\n").arg(tomlDate(config.created));
    text += QStringLiteral("\n[tex]\npreamble = %1\n").arg(tomlStringArray(config.preamble));
    for (const ProjectTag& tag : config.tags) {
        text += QStringLiteral("\n[[tags]]\nid = %1\nkind = %2\ntitle = %3\n")
                    .arg(tomlBasicString(tag.id), tomlBasicString(tag.kind), tomlBasicString(tag.title));
    }
    return text.toUtf8();
}

OpenedProject openProject(const QString& folder)
{
    OpenedProject project;
    project.root = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
    const QString nativeRoot = QDir::toNativeSeparators(project.root);

    const QFileInfo tomlInfo(project.root + QStringLiteral("/loom.toml"));
    if (!tomlInfo.exists() && !tomlInfo.isSymbolicLink()) {
        project.status = OpenStatus::NotAProject;
        project.message = QStringLiteral("`%1` isn't a Loom project: it has no loom.toml.").arg(nativeRoot);
        return project;
    }

    const std::optional<QByteArray> bytes = readProjectFile(project.root, QStringLiteral("loom.toml"));
    if (!bytes) {
        project.status = OpenStatus::Unreadable;
        project.message = QStringLiteral("Loom couldn't read `%1`.").arg(QDir::toNativeSeparators(tomlInfo.absoluteFilePath()));
        return project;
    }

    ProjectTomlReading reading = readProjectToml(*bytes, project.root);
    project.problems = std::move(reading.problems);
    if (reading.formatTooNew) {
        project.status = OpenStatus::FormatTooNew;
        project.message = project.problems.front().message;
        return project;
    }
    project.config = std::move(reading.config);
    project.status = OpenStatus::Opened;
    return project;
}

} // namespace loom
