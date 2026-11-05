#include "LoomCore/Checks.h"

#include "LoomCore/FrontMatter.h"
#include "LoomCore/Fingerprint.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/TomlFields.h"
#include "LoomCore/TomlWriter.h"

#include <QRegularExpression>
#include <QTimeZone>

#include <algorithm>
#include <string>
#include <string_view>

namespace loom {

namespace {

constexpr auto checkFieldList = u"by, date, fingerprint, verdict, changes, record, note";

/// Reads one entry of a check history, recording where its fields are and whether anything had
/// to be dropped.
struct EntryReader {
    FieldChecker& checker;
    const toml::table& table;
    CheckLines lines;
    /// "Check {n}", for the messages of the spec's Checks table.
    QString name;
    bool lost = false;

    /// Reads a string field. A missing required field is reported at the entry's header. Every
    /// string field here needs text: an empty one, or one of only whitespace, is `missing-field`
    /// (Specs/m1-core.md, "Front matter", A2, A21), except `changes`, which is free text and
    /// keeps its `wrong-type` for either. The value is kept as written, so rewriting the entry
    /// (merge-checks) changes nothing in it.
    /// @param required Whether every check needs the field (`by`, `fingerprint`, `verdict`).
    /// @returns The value as written; empty if absent or not a string.
    QString string(const char* key, bool required)
    {
        const QString field = QString::fromLatin1(key);
        const toml::node* node = table.get(key);
        if (!node) {
            if (required)
                checker.missingField(field, lines.header, QStringLiteral("check"));
            return {};
        }
        lines.fields.insert(field, checker.line(*node));
        const toml::value<std::string>* value = node->as_string();
        if (!value) {
            checker.wrongType(field, QStringLiteral("a string"), *node);
            lost = true;
            return {};
        }
        const Need need = field == u"changes" ? Need::Optional : required ? Need::Required : Need::Naming;
        if (checker.reportEmpty(field, *node, QStringLiteral("check"), need))
            return fromUtf8(value->get()); // Kept as written: entries are never changed.
        if (value->get().empty())
            checker.wrongType(field, QStringLiteral("a non-empty string"), *node);
        else if (isBlankBody(QByteArray::fromStdString(value->get())))
            checker.add("wrong-type", checker.line(*node),
                QStringLiteral("`%1` should be a non-empty string, not only whitespace.").arg(field));
        return fromUtf8(value->get());
    }

    /// Reads `date`: an offset date-time to the second (Specs/m1-core.md, D1).
    QDateTime date()
    {
        const QString field = QStringLiteral("date");
        const toml::node* node = table.get("date");
        if (!node) {
            checker.missingField(field, lines.header, QStringLiteral("check"));
            return {};
        }
        lines.fields.insert(field, checker.line(*node));
        if (checker.reportEmpty(field, *node, QStringLiteral("check"), Need::Required)) {
            // Dropped, as any `date` Loom can't read is: rewriting would change the entry, and
            // entries are never changed (Specs/m1-core.md, "Checks").
            lost = true;
            return {};
        }
        const toml::value<toml::date_time>* value = node->as_date_time();
        if (!value || !value->get().offset || value->get().time.nanosecond != 0) {
            checker.add("wrong-type", checker.line(*node),
                QStringLiteral("`date` should be a date and time to the second with its offset from UTC, like "
                               "2026-11-04T15:32:10-06:00, not %1.")
                    .arg(value && !value->get().offset ? QStringLiteral("a date and time without an offset")
                         : value ? QStringLiteral("a time with fractions of a second") : describeToml(*node)));
            lost = true;
            return {};
        }
        const toml::date_time& dt = value->get();
        // A9: a date Loom can't hold (the year 0) is `bad-value` in any file. It is dropped like
        // any `date` Loom can't read, so the check is undated and never counts (standingOf()).
        const QDate day = checker.date(field, dt.date, checker.line(*node));
        if (!day.isValid()) {
            lost = true;
            return {};
        }
        return QDateTime(day, QTime(dt.time.hour, dt.time.minute, dt.time.second),
                         QTimeZone::fromSecondsAheadOfUtc(dt.offset->minutes * 60));
    }

    /// Reports a path field that isn't a project path (`bad-path`).
    void path(const QString& field, const QString& value)
    {
        if (!value.isEmpty() && !isProjectPath(value))
            checker.add("bad-path", lines.of(field),
                QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.").arg(value));
    }
};

/// Reads the entry `table`, the `number`th (from 1), into `reading`.
void readEntry(const toml::table& table, int number, FieldChecker& checker, ChecksReading& reading, bool& lost)
{
    EntryReader entry{checker, table, {}, QStringLiteral("Check %1").arg(number), false};
    entry.lines.header = checker.line(table);
    {
        const std::size_t before = checker.problems.size();
        checker.unknownFields(table, {"by", "date", "fingerprint", "verdict", "changes", "record", "note"},
                              QString(), QStringLiteral("a check"), checkFieldList);
        entry.lost = checker.problems.size() != before;
    }

    Check check;
    check.by = entry.string("by", true);
    check.date = entry.date();
    check.fingerprint = entry.string("fingerprint", true);
    check.verdict = entry.string("verdict", true);
    check.changes = entry.string("changes", false);
    check.record = entry.string("record", false);
    check.note = entry.string("note", false);

    if (!check.by.isEmpty() && !isCheckAuthor(check.by))
        checker.add("check-by", entry.lines.of(QStringLiteral("by")),
            QStringLiteral("%1: `by` must be `dakota` or `agent:<model>`, not `%2`.").arg(entry.name, check.by));
    if (!check.fingerprint.isEmpty() && !isFingerprint(check.fingerprint))
        checker.add("check-fingerprint", entry.lines.of(QStringLiteral("fingerprint")),
            QStringLiteral("%1: the fingerprint must be 12 characters from 0–9 and a–f.").arg(entry.name));
    if (!check.verdict.isEmpty() && !checkVerdicts.contains(check.verdict))
        checker.add("check-verdict", entry.lines.of(QStringLiteral("verdict")),
            QStringLiteral("%1: `%2` isn't a verdict. Use pass, needs-changes or fail.").arg(entry.name, check.verdict));

    const bool needsChanges = check.verdict == u"needs-changes";
    if (!check.changes.isEmpty() && checkVerdicts.contains(check.verdict) && !needsChanges)
        checker.add("check-changes-unexpected", entry.lines.of(QStringLiteral("changes")),
            QStringLiteral("%1 has `changes`, but only a needs-changes check says what to change.").arg(entry.name));
    if (check.changes.isEmpty() && needsChanges && check.byDakota())
        checker.add("check-changes-missing", entry.lines.header,
            QStringLiteral("%1 is Dakota's needs-changes but doesn't say what to change. Add `changes`.").arg(entry.name));
    if (check.record.isEmpty() && check.byAgent())
        checker.add("check-record-missing", entry.lines.header,
            QStringLiteral("%1 is by an agent and has no `record`: every agent check links its reasoning.").arg(entry.name));
    entry.path(QStringLiteral("record"), check.record);
    entry.path(QStringLiteral("note"), check.note);

    lost = lost || entry.lost;
    reading.checks.push_back(check);
    reading.lines.push_back(entry.lines);
}

/// Whether `a` is older than `b`: an earlier date. Equal dates aren't older either way, so a
/// stable sort keeps file order for them (Specs/m1-core.md, "Checks", rule 2).
bool olderDate(const Check& a, const Check& b)
{
    return a.date < b.date;
}

} // namespace

bool sameCheck(const Check& a, const Check& b)
{
    return a.by == b.by && a.date == b.date && a.date.offsetFromUtc() == b.date.offsetFromUtc()
        && a.fingerprint == b.fingerprint && a.verdict == b.verdict && a.changes == b.changes
        && a.record == b.record && a.note == b.note;
}

bool isCheckAuthor(const QString& by)
{
    // Anchored at the very end: Qt's `$` also matches before a final line break.
    static const QRegularExpression agent(QRegularExpression::anchoredPattern(QStringLiteral("agent:[a-z0-9][a-z0-9.-]*")));
    return by == u"dakota" || agent.match(by).hasMatch();
}

ChecksReading readChecks(const QByteArray& bytes, const QString& path)
{
    ChecksReading reading;
    FieldChecker checker{path, 0, {}};
    if (const std::optional<qsizetype> bad = firstInvalidUtf8(bytes)) {
        checker.add("not-utf8", 1,
            QStringLiteral("This file isn't valid UTF-8 (the first bad byte is at offset %1). Save it as UTF-8.").arg(*bad));
        reading.problems = std::move(checker.problems);
        return reading;
    }

    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), path.toStdString());
    } catch (const toml::parse_error& error) {
        QString description = fromUtf8(error.description());
        while (description.endsWith(u'.'))
            description.chop(1);
        checker.add("toml-syntax", lineOf(error.source()), QStringLiteral("This isn't valid TOML: %1.").arg(description));
        reading.problems = std::move(checker.problems);
        return reading;
    }
    reading.parsed = true;

    bool lost = false;
    checker.unknownFields(table, {"checks"}, QString(), QStringLiteral("a check history"), u"checks");
    lost = !checker.problems.empty();

    if (const toml::node* node = table.get("checks")) {
        const toml::array* array = node->as_array();
        if (!array) {
            checker.wrongType(QStringLiteral("checks"), QStringLiteral("a list of checks, each `[[checks]]`"), *node);
            lost = true;
        } else {
            int number = 0;
            for (const toml::node& element : *array) {
                ++number;
                if (const toml::table* entry = element.as_table()) {
                    readEntry(*entry, number, checker, reading, lost);
                } else {
                    checker.add("wrong-type", checker.line(element),
                        QStringLiteral("`checks` should be a list of checks, each `[[checks]]`, not a list holding %1.")
                            .arg(describeToml(element)));
                    lost = true;
                }
            }
        }
    }

    // Across entries: date order and repeats (Specs/m1-core.md, "Checks", rule 2).
    for (std::size_t i = 1; i < reading.checks.size(); ++i) {
        const QDateTime& previous = reading.checks[i - 1].date;
        const QDateTime& current = reading.checks[i].date;
        if (previous.isValid() && current.isValid() && current < previous)
            checker.add("check-order", reading.lines[i].of(QStringLiteral("date")),
                QStringLiteral("Check %1 is dated before check %2. Checks are kept in date order.").arg(i + 1).arg(i));
    }
    for (std::size_t i = 1; i < reading.checks.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (sameCheck(reading.checks[j], reading.checks[i])) {
                checker.add("check-duplicate", reading.lines[i].header,
                    QStringLiteral("Checks %1 and %2 are the same entry twice.").arg(j + 1).arg(i + 1));
                break;
            }
        }
    }

    reading.lossless = !lost;
    reading.problems = std::move(checker.problems);
    sortProblems(reading.problems);
    return reading;
}

QByteArray formatCheck(const Check& check)
{
    QString text = QStringLiteral("[[checks]]\n");
    const auto line = [&](const char* key, const QString& value) {
        if (!value.isEmpty())
            text += QString::fromLatin1(key) + QStringLiteral(" = ") + value + u'\n';
    };
    line("by", check.by.isEmpty() ? QString() : tomlBasicString(check.by));
    line("date", check.date.isValid() ? tomlOffsetDateTime(check.date) : QString());
    line("fingerprint", check.fingerprint.isEmpty() ? QString() : tomlBasicString(check.fingerprint));
    line("verdict", check.verdict.isEmpty() ? QString() : tomlBasicString(check.verdict));
    if (!check.changes.isEmpty())
        line("changes", check.changes.contains(u'\n') ? tomlMultilineStringAsIs(check.changes) : tomlBasicString(check.changes));
    line("record", check.record.isEmpty() ? QString() : tomlBasicString(check.record));
    line("note", check.note.isEmpty() ? QString() : tomlBasicString(check.note));
    return text.toUtf8();
}

QByteArray formatChecks(const std::vector<Check>& checks)
{
    QByteArray out;
    for (const Check& check : checks) {
        if (!out.isEmpty())
            out += '\n';
        out += formatCheck(check);
    }
    return out;
}

CheckStanding standingOf(const std::vector<Check>& checks, const QString& currentFingerprint)
{
    CheckStanding standing;
    // Newest = latest date, and for equal dates the later in the file (rule 2). Scanning in file
    // order and taking any entry at least as new as the one held gives exactly that.
    const auto newer = [&](qsizetype candidate, const std::optional<qsizetype>& held) {
        return !held || !(checks[candidate].date < checks[*held].date);
    };
    for (qsizetype i = 0; i < static_cast<qsizetype>(checks.size()); ++i) {
        const Check& check = checks[i];
        if (currentFingerprint.isEmpty() || check.fingerprint != currentFingerprint) // Rule 4: only matching checks count.
            continue;
        // A check without a date Loom can read (missing, wrong or the year 0, which validation
        // reports) has no place in the date order, so it is never the newest of anything (A22).
        if (!check.date.isValid())
            continue;
        if (check.byAgent() && newer(i, standing.countingAgent))
            standing.countingAgent = i;
        else if (check.byDakota() && newer(i, standing.countingDakota))
            standing.countingDakota = i;
    }

    const Check* agent = standing.countingAgent ? &checks[*standing.countingAgent] : nullptr;
    const Check* dakota = standing.countingDakota ? &checks[*standing.countingDakota] : nullptr;
    // Rule 6: Dakota's pass confirms the agent's, so it must be the newer of the two.
    standing.checked = agent && dakota && agent->verdict == u"pass" && dakota->verdict == u"pass"
        && (agent->date < dakota->date || (agent->date == dakota->date && *standing.countingAgent < *standing.countingDakota));
    // Rule 7: a counting fail from either means the proof doesn't support its statement.
    standing.supports = !(agent && agent->verdict == u"fail") && !(dakota && dakota->verdict == u"fail");
    return standing;
}

ChecksMerge mergeChecks(const QByteArray& base, const QByteArray& ours, const QByteArray& theirs)
{
    ChecksMerge merge;
    const ChecksReading readings[] = {readChecks(base, QStringLiteral("base")), readChecks(ours, QStringLiteral("ours")),
                                      readChecks(theirs, QStringLiteral("theirs"))};
    const char* const names[] = {"the common ancestor's", "our", "their"};
    for (int i = 0; i < 3; ++i) {
        const ChecksReading& reading = readings[i];
        const bool undated = std::any_of(reading.checks.begin(), reading.checks.end(), [](const Check& c) { return !c.date.isValid(); });
        if (!reading.lossless || undated) {
            merge.message = QStringLiteral("Loom couldn't merge the check histories: %1 version isn't one it can read in full")
                                .arg(QString::fromLatin1(names[i]));
            if (!reading.problems.empty())
                merge.message += QStringLiteral(" (line %1: %2)").arg(reading.problems.front().line).arg(reading.problems.front().message);
            merge.message += QStringLiteral(". Resolve the conflict by hand, keeping every check.");
            return merge;
        }
    }
    const std::vector<Check>& baseChecks = readings[0].checks;
    const std::vector<Check>& ourChecks = readings[1].checks;
    const std::vector<Check>& theirChecks = readings[2].checks;

    const auto contains = [](const std::vector<Check>& list, const Check& check) {
        return std::any_of(list.begin(), list.end(), [&](const Check& c) { return sameCheck(c, check); });
    };
    std::vector<Check> merged = ourChecks;
    for (const Check& check : theirChecks) {
        if (!contains(merged, check))
            merged.push_back(check);
    }
    // Rule 1 of Checks: nothing is ever removed, so a base entry one side deleted comes back.
    for (const Check& check : baseChecks) {
        if (!contains(ourChecks, check) || !contains(theirChecks, check))
            merge.restored = true;
        if (!contains(merged, check))
            merged.push_back(check);
    }
    std::stable_sort(merged.begin(), merged.end(), olderDate);

    merge.ok = true;
    merge.merged = formatChecks(merged);
    return merge;
}

} // namespace loom
