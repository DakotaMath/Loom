#include "LoomCore/ProjectToml.h"

#include "LoomCore/FieldReader.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/TomlFields.h"
#include "LoomCore/TomlWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <optional>
#include <string_view>

namespace loom {

namespace {

/// The fields of `loom.toml`, as the `unknown-field` message lists them.
constexpr auto projectFieldList = u"format, name, idea, created, tex.preamble, tags";
/// The fields of a tag, likewise.
constexpr auto tagFieldList = u"id, kind, title";

/// A FieldChecker for loom.toml, a plain TOML file: its lines are the TOML's lines.
FieldChecker makeChecker()
{
    return FieldChecker{QStringLiteral("loom.toml"), 0, {}};
}

/// Reads `tex.preamble`: a list of project paths to existing `.tex` files.
void readPreamble(FieldChecker& checker, const toml::table& root, const QString& projectRoot, ProjectConfig& config)
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
                QStringLiteral("`tex.preamble` should be a list of paths, not a list holding %1.").arg(describeToml(entry)));
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
void readTags(FieldChecker& checker, const toml::table& root, ProjectConfig& config)
{
    const toml::node* tagsNode = root.get("tags");
    if (!tagsNode)
        return;
    const toml::array* tags = tagsNode->as_array();
    if (!tags) {
        checker.wrongType(QStringLiteral("tags"), QStringLiteral("a list of tables"), *tagsNode);
        return;
    }

    // Anchored at the very end: Qt's `$` also matches before a final line break.
    static const QRegularExpression tagSyntax(QRegularExpression::anchoredPattern(QStringLiteral("[a-z0-9]+(-[a-z0-9]+)*")));
    const QString thing = QStringLiteral("tag");
    QSet<QString> seen;
    for (const toml::node& entry : *tags) {
        const toml::table* tag = entry.as_table();
        if (!tag) {
            checker.add("wrong-type", lineOf(entry.source()),
                QStringLiteral("`tags` should be a list of tables, not a list holding %1.").arg(describeToml(entry)));
            continue;
        }
        const int line = lineOf(tag->source());
        checker.unknownFields(*tag, {"id", "kind", "title"}, QString(), QStringLiteral("a tag"), tagFieldList);

        const std::optional<QString> id = checker.stringField(*tag, "id", QStringLiteral("id"), line, thing, Need::Required);
        const std::optional<QString> kind = checker.stringField(*tag, "kind", QStringLiteral("kind"), line, thing, Need::Required);
        const std::optional<QString> title = checker.stringField(*tag, "title", QStringLiteral("title"), line, thing, Need::Required);

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
    FieldChecker checker = makeChecker();

    // The same UTF-8 check as every other file (`not-utf8`), then TOML (`toml-syntax`).
    std::optional<toml::table> parsed = parsePlainToml(bytes, checker);
    if (!parsed) {
        reading.problems = std::move(checker.problems);
        return reading;
    }
    const toml::table& table = *parsed;

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

    if (const std::optional<QString> name = checker.stringField(table, "name", QStringLiteral("name"), 1, project, Need::Required))
        config.name = *name;
    if (const std::optional<QString> idea = checker.stringField(table, "idea", QStringLiteral("idea"), 1, project, Need::Optional))
        config.idea = *idea;

    if (const toml::node* node = table.get("created")) {
        if (node->as_date()) {
            // A date QDate can't hold (the year 0) is reported, not dropped without a word.
            config.created = checker.date(QStringLiteral("created"), *node);
        } else if (!checker.reportEmpty(QStringLiteral("created"), *node, project, Need::Required)) {
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
