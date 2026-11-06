#include "LoomCore/ReviewChanges.h"

#include "LoomCore/Checks.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"

#include <QSet>

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

namespace loom {

namespace {

const QString arrow = QStringLiteral(u" → ");
const QString dot = QStringLiteral(u" · ");
const QString minus = QStringLiteral(u"−");

/// The file of `files` at `path`, or null.
template <typename File>
const File* fileAt(const std::vector<File>& files, const QString& path)
{
    const auto it = std::find_if(files.begin(), files.end(), [&](const File& file) { return file.path == path; });
    return it == files.end() ? nullptr : &*it;
}

/// Puts `source`'s file at `path` (or none) in place of `into`'s, keeping the list sorted by path.
template <typename File>
void takeFile(std::vector<File>& into, const std::vector<File>& source, const QString& path)
{
    std::erase_if(into, [&](const File& file) { return file.path == path; });
    if (const File* file = fileAt(source, path)) {
        into.push_back(*file);
        std::sort(into.begin(), into.end(), [](const File& a, const File& b) { return a.path < b.path; });
    }
}

/// `base` with the one file at `path` as `source` has it: what outdatedChecks() compares to see
/// which checks this file's change alone outdates.
ProjectContents withFileFrom(const ProjectContents& base, const ProjectContents& source, const QString& path)
{
    ProjectContents mixed = base;
    takeFile(mixed.statements, source.statements, path);
    takeFile(mixed.proofs, source.proofs, path);
    takeFile(mixed.verbose, source.verbose, path);
    takeFile(mixed.checks, source.checks, path);
    takeFile(mixed.papers, source.papers, path);
    takeFile(mixed.stubs, source.stubs, path);
    return mixed;
}

/// The title of a statement or stub (a stub without one: its gist), from `after`, else `before`;
/// empty if neither has it.
QString titleOf(const ProjectContents& before, const ProjectContents& after, const QString& id)
{
    for (const ProjectContents* contents : {&after, &before}) {
        if (const StatementFile* statement = contents->statement(id))
            return statement->reading.document.title;
        if (const StubFile* stub = contents->stub(id))
            return stub->reading.document.title.isEmpty() ? stub->reading.document.gist : stub->reading.document.title;
    }
    return {};
}

/// An ID with its title, as every line names IDs: `4KX7 · Energy decay`, or the ID alone when it
/// has no title (a proof, or an ID that isn't there).
QString named(const ProjectContents& before, const ProjectContents& after, const QString& id)
{
    const QString title = titleOf(before, after, id);
    return title.isEmpty() ? id : id + dot + title;
}

/// A string as the lines quote it.
QString quoted(const QString& text)
{
    return QStringLiteral("\"%1\"").arg(text);
}

/// `name: old → new`, with `(none)` for an absent value.
QString fromTo(const QString& name, const QString& before, const QString& after)
{
    const auto shown = [](const QString& value) { return value.isEmpty() ? QStringLiteral("(none)") : value; };
    return name + QStringLiteral(": ") + shown(before) + arrow + shown(after);
}

/// A date as the lines write it; empty for an invalid one.
QString dateText(const QDate& date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

/// `name: +a, +b, −c`: what was added (in `after`'s order), then what was removed (in `before`'s);
/// empty if the lists hold the same items. A change of order alone isn't a change.
QString listChange(const QString& name, const QStringList& before, const QStringList& after,
                   const std::function<QString(const QString&)>& label)
{
    QStringList parts;
    for (const QString& item : after)
        if (!before.contains(item))
            parts << QStringLiteral("+") + label(item);
    for (const QString& item : before)
        if (!after.contains(item))
            parts << minus + label(item);
    return parts.isEmpty() ? QString() : name + QStringLiteral(": ") + parts.join(QStringLiteral(", "));
}

/// Who made a check, in words: `Dakota`, `agent claude-opus-5-5`.
QString checker(const QString& by)
{
    if (by == u"dakota")
        return QStringLiteral("Dakota");
    if (by.startsWith(u"agent:"))
        return QStringLiteral("agent ") + by.mid(6);
    return by;
}

/// What builds the lines of one file.
class Lines {
public:
    explicit Lines(std::vector<ChangeLine>& lines)
        : m_lines(lines)
    {
    }
    /// Adds a line, unless it's empty.
    void add(const QString& text, const QString& what, bool warning = false)
    {
        if (!text.isEmpty())
            m_lines.push_back({text, what, warning});
    }
    /// Adds `name: old → new` if the values differ.
    void field(const QString& name, const QString& before, const QString& after, const QString& what = {})
    {
        if (before != after)
            add(fromTo(name, before, after), what.isEmpty() ? name : what);
    }
    /// Adds a quoted `name: "old" → "new"` if the values differ.
    void text(const QString& name, const QString& before, const QString& after)
    {
        if (before != after)
            add(fromTo(name, before.isEmpty() ? QString() : quoted(before), after.isEmpty() ? QString() : quoted(after)), name);
    }

private:
    std::vector<ChangeLine>& m_lines;
};

/// The id a statement, proof or stub file names, for M1's `deleted-since-commit` warning.
void deletedLines(Lines& lines, const QString& id)
{
    lines.add(QStringLiteral("deleted"), QStringLiteral("deleted"));
    lines.add(QStringLiteral("%1 was in the last commit but its file is gone. Tags are permanent: supersede a statement, "
                             "or mark a proof `failed`, instead of deleting it.")
                  .arg(id),
              QStringLiteral("deleted"), true);
}

/// The plain lines for a file Loom doesn't read, or can't read on one side.
void plainFile(Lines& lines, FileChange change)
{
    switch (change) {
    case FileChange::New:
        lines.add(QStringLiteral("new"), QStringLiteral("new"));
        break;
    case FileChange::Deleted:
        lines.add(QStringLiteral("deleted"), QStringLiteral("deleted"));
        break;
    case FileChange::Changed:
        lines.add(QStringLiteral("changed"), QStringLiteral("changed"));
        break;
    }
}

/// `text changed`, and the checks this file's change alone outdates.
void bodyLine(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path)
{
    const QStringList outdated = outdatedChecks(before, withFileFrom(before, after, path));
    lines.add(outdated.isEmpty() ? QStringLiteral("text changed")
                                 : QStringLiteral("text changed; outdates the checks of %1").arg(outdated.join(QStringLiteral(", "))),
              QStringLiteral("text"));
}

void statementLines(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    const StatementFile* old = fileAt(before.statements, path);
    const StatementFile* now = fileAt(after.statements, path);
    if (!now) {
        if (old)
            deletedLines(lines, old->tag);
        else
            plainFile(lines, change);
        return;
    }
    const auto label = [&](const QString& id) { return named(before, after, id); };
    if (!now->reading.parsed) {
        lines.add(QStringLiteral("the front matter can't be read; the Problems panel says why"), QStringLiteral("front matter"), true);
        return;
    }
    if (!old || !old->reading.parsed) {
        lines.add(QStringLiteral("new %1 %2").arg(now->reading.document.kind, label(now->tag)), QStringLiteral("new"));
        return;
    }
    const Statement& b = old->reading.document;
    const Statement& a = now->reading.document;
    lines.field(QStringLiteral("id"), b.id, a.id);
    lines.field(QStringLiteral("kind"), b.kind, a.kind);
    lines.text(QStringLiteral("title"), b.title, a.title);
    lines.add(listChange(QStringLiteral("tags"), b.tags, a.tags, [](const QString& tag) { return tag; }), QStringLiteral("tags"));
    lines.add(listChange(QStringLiteral("stated using"), b.statedUsing, a.statedUsing, label), QStringLiteral("stated using"));
    lines.add(listChange(QStringLiteral("aliases"), b.aliases, a.aliases, [](const QString& alias) { return alias; }), QStringLiteral("aliases"));
    // An absent origin means `new` (Specs/m1-core.md, "Statements").
    lines.field(QStringLiteral("origin"), b.origin.isEmpty() ? QStringLiteral("new") : b.origin,
                a.origin.isEmpty() ? QStringLiteral("new") : a.origin);
    if (b.supersededBy != a.supersededBy)
        lines.add(a.supersededBy.isEmpty() ? QStringLiteral("no longer superseded") : QStringLiteral("superseded by ") + label(a.supersededBy),
                  QStringLiteral("superseded"));
    if (b.refutedBy != a.refutedBy || b.refutedOn != a.refutedOn) {
        if (a.refutedBy.isEmpty())
            lines.add(QStringLiteral("no longer refuted"), QStringLiteral("refuted"));
        else if (a.refutedOn.isValid())
            lines.add(QStringLiteral("refuted by %1 (accepted %2)").arg(a.refutedBy, dateText(a.refutedOn)), QStringLiteral("refuted"));
        else
            lines.add(QStringLiteral("refuted by %1 (not yet accepted)").arg(a.refutedBy), QStringLiteral("refuted"));
    }
    lines.field(QStringLiteral("created"), dateText(b.created), dateText(a.created));
    if (b.verified != a.verified)
        lines.add(a.verified.isValid() ? QStringLiteral("front matter verified ") + dateText(a.verified)
                                       : QStringLiteral("front matter no longer verified"),
                  QStringLiteral("verified"));
    if (b.body != a.body)
        bodyLine(lines, before, after, path);
}

void proofLines(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    const ProofFile* old = fileAt(before.proofs, path);
    const ProofFile* now = fileAt(after.proofs, path);
    if (!now) {
        if (old)
            deletedLines(lines, old->id);
        else
            plainFile(lines, change);
        return;
    }
    const auto label = [&](const QString& id) { return named(before, after, id); };
    if (!now->reading.parsed) {
        lines.add(QStringLiteral("the front matter can't be read; the Problems panel says why"), QStringLiteral("front matter"), true);
        return;
    }
    const Proof& a = now->reading.document;
    if (!old || !old->reading.parsed) {
        QStringList premises;
        for (const QString& premise : a.premises)
            premises << label(premise);
        lines.add(premises.isEmpty() ? QStringLiteral("new proof %1 (%2)").arg(now->id, a.level)
                                     : QStringLiteral("new proof %1 (%2, from %3)").arg(now->id, a.level, premises.join(QStringLiteral(", "))),
                  QStringLiteral("new"));
        return;
    }
    const Proof& b = old->reading.document;
    lines.field(QStringLiteral("id"), b.id, a.id);
    lines.field(QStringLiteral("proves"), b.proves, a.proves);
    lines.add(listChange(QStringLiteral("premises"), b.premises, a.premises, label), QStringLiteral("premises"));
    lines.field(QStringLiteral("level"), b.level, a.level);
    lines.field(QStringLiteral("created"), dateText(b.created), dateText(a.created));
    if (b.verified != a.verified)
        lines.add(a.verified.isValid() ? QStringLiteral("front matter verified ") + dateText(a.verified)
                                       : QStringLiteral("front matter no longer verified"),
                  QStringLiteral("verified"));
    if (b.body != a.body)
        bodyLine(lines, before, after, path);
}

void verboseLines(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    const VerboseFile* old = fileAt(before.verbose, path);
    const VerboseFile* now = fileAt(after.verbose, path);
    if (!old && !now) {
        plainFile(lines, change);
        return;
    }
    const QString what = QStringLiteral("verbose");
    if (!now) {
        lines.add(QStringLiteral("verbose version removed"), what);
        return;
    }
    if (!old) {
        lines.add(QStringLiteral("verbose version added"), what);
        return;
    }
    if (old->reading.document.body != now->reading.document.body)
        lines.add(QStringLiteral("verbose version changed"), what);
    else if (old->reading.document.fingerprint != now->reading.document.fingerprint)
        lines.add(QStringLiteral("verbose version marked up to date"), what);
}

void checksLines(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    const ChecksFile* old = fileAt(before.checks, path);
    const ChecksFile* now = fileAt(after.checks, path);
    if ((!old || !old->reading.parsed) && (!now || !now->reading.parsed)) {
        plainFile(lines, change);
        return;
    }
    if (now && !now->reading.parsed) {
        lines.add(QStringLiteral("the check history can't be read; the Problems panel says why"), QStringLiteral("check"), true);
        return;
    }
    const std::vector<Check> none;
    const std::vector<Check>& committed = old && old->reading.parsed ? old->reading.checks : none;
    const std::vector<Check>& current = now ? now->reading.checks : none;
    const QString what = QStringLiteral("check");

    // Committed checks are matched in order, each to the first equal current check after the
    // previous match, as M1's `check-changed` matches them (Validation.cpp, checkAgainstCommit()).
    std::vector<bool> used(current.size(), false);
    std::size_t next = 0;
    for (const Check& check : committed) {
        std::size_t j = next;
        while (j < current.size() && !sameCheck(check, current[j]))
            ++j;
        if (j < current.size()) {
            used[j] = true;
            next = j + 1;
            continue;
        }
        lines.add(QStringLiteral("check changed: %1, %2")
                      .arg(checker(check.by), check.date.isValid() ? check.date.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("no date")),
                  what, true);
    }
    const QString proofId = now ? now->proofId : old->proofId;
    const QString fingerprint = proofChecking(after, proofId).fingerprint;
    for (std::size_t j = 0; j < current.size(); ++j) {
        if (used[j])
            continue;
        const Check& check = current[j];
        QString text = QStringLiteral("check: %1 %2").arg(checker(check.by), check.verdict);
        if (!check.changes.isEmpty())
            text += QStringLiteral(": ") + quoted(check.changes.section(QLatin1Char('\n'), 0, 0).trimmed());
        text += !fingerprint.isEmpty() && check.fingerprint == fingerprint ? QStringLiteral(" (this version)")
                                                                           : QStringLiteral(" (an older version)");
        lines.add(text, what);
    }
}

/// A paper field's value as the lines show it: strings quoted, the rest as text.
QString fieldValue(const std::optional<PaperField>& field)
{
    if (!field)
        return {};
    return std::holds_alternative<QString>(field->value) ? quoted(field->text()) : field->text();
}

/// The lines for one bibliographic field: its value, its source and its `verified`.
void paperField(Lines& lines, const QString& name, const std::optional<PaperField>& before, const std::optional<PaperField>& after)
{
    lines.field(name, fieldValue(before), fieldValue(after), name);
    const QString beforeSource = before ? before->source : QString();
    const QString afterSource = after ? after->source : QString();
    lines.field(name + QStringLiteral(" source"), beforeSource, afterSource, name);
    const QDate beforeVerified = before ? before->verified : QDate();
    const QDate afterVerified = after ? after->verified : QDate();
    if (beforeVerified != afterVerified && after)
        lines.add(afterVerified.isValid() ? QStringLiteral("%1 verified %2").arg(name, dateText(afterVerified))
                                          : QStringLiteral("%1 no longer verified").arg(name),
                  name);
}

/// A version's field, or nothing.
std::optional<PaperField> versionField(const PaperVersion* version, const QString& name)
{
    if (!version)
        return std::nullopt;
    if (const PaperField* field = version->field(name))
        return *field;
    return std::nullopt;
}

/// A reading entry in a few words: `read arxiv-v1, 2026-11-05 (Section 2)`.
QString readingText(const ReadingEntry& reading)
{
    QString text = QStringLiteral("%1 %2, %3").arg(reading.level, reading.version, dateText(reading.date));
    if (!reading.what.isEmpty())
        text += QStringLiteral(" (%1)").arg(reading.what);
    if (!reading.session.isEmpty())
        text += QStringLiteral(", ") + reading.session;
    return text;
}

/// A renumbering in a few words: `arxiv-v1 2.1, published 3.1`, with `(checked …)`.
QString renumberingText(const Renumbering& renumbering)
{
    QStringList labels;
    for (const auto& [version, label] : renumbering.labels)
        labels << QStringLiteral("%1 %2").arg(version, label);
    QString text = labels.join(QStringLiteral(", "));
    if (renumbering.checked.isValid())
        text += QStringLiteral(" (checked %1)").arg(dateText(renumbering.checked));
    return text;
}

/// Adds `<what> added: …` and `<what> removed: …` for the items of two lists, compared as text.
void itemLines(Lines& lines, const QString& what, const QStringList& before, const QStringList& after)
{
    QStringList left = before;
    for (const QString& item : after) {
        if (const qsizetype at = left.indexOf(item); at >= 0)
            left.removeAt(at);
        else
            lines.add(QStringLiteral("%1 added: %2").arg(what, item), what);
    }
    for (const QString& item : left)
        lines.add(QStringLiteral("%1 removed: %2").arg(what, item), what);
}

void paperLines(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    const PaperFile* old = fileAt(before.papers, path);
    const PaperFile* now = fileAt(after.papers, path);
    if (!now || !now->reading.parsed || !old || !old->reading.parsed) {
        if (now && now->reading.parsed && (!old || !old->reading.parsed)) {
            const Paper& a = now->reading.document;
            const QString title = a.work.title ? a.work.title->text() : QString();
            lines.add(title.isEmpty() ? QStringLiteral("new paper %1").arg(a.key) : QStringLiteral("new paper %1%2%3").arg(a.key, dot, title),
                      QStringLiteral("new"));
        } else if (now && !now->reading.parsed) {
            lines.add(QStringLiteral("paper.toml can't be read; the Problems panel says why"), QStringLiteral("paper.toml"), true);
        } else {
            plainFile(lines, change);
        }
        return;
    }
    const Paper& b = old->reading.document;
    const Paper& a = now->reading.document;
    const auto same = [](const QString& item) { return item; };
    lines.field(QStringLiteral("key"), b.key, a.key);
    lines.add(listChange(QStringLiteral("aliases"), b.aliases, a.aliases, same), QStringLiteral("aliases"));
    lines.field(QStringLiteral("cite"), b.cite, a.cite);
    lines.field(QStringLiteral("relevance"), b.relevance ? QString::number(b.relevance) : QString(),
                a.relevance ? QString::number(a.relevance) : QString());
    lines.text(QStringLiteral("relevance why"), b.relevanceWhy, a.relevanceWhy);
    lines.add(listChange(QStringLiteral("tags"), b.tags, a.tags, same), QStringLiteral("tags"));
    lines.text(QStringLiteral("summary"), b.summary, a.summary);
    lines.field(QStringLiteral("created"), dateText(b.created), dateText(a.created));
    if (b.verified != a.verified)
        lines.add(a.verified.isValid() ? QStringLiteral("entry verified ") + dateText(a.verified) : QStringLiteral("entry no longer verified"),
                  QStringLiteral("verified"));

    paperField(lines, QStringLiteral("type"), b.work.type, a.work.type);
    paperField(lines, QStringLiteral("title"), b.work.title, a.work.title);
    paperField(lines, QStringLiteral("authors"), b.work.authors, a.work.authors);
    paperField(lines, QStringLiteral("editors"), b.work.editors, a.work.editors);

    // Versions by ID: added, removed, and the fields of those in both (named with the version's ID
    // when the paper has several, so `journal` says whose it is).
    const bool several = std::max(a.versions.size(), b.versions.size()) > 1;
    for (const PaperVersion& version : a.versions)
        if (!b.version(version.id))
            lines.add(QStringLiteral("version %1 added").arg(version.id), QStringLiteral("version"));
    for (const PaperVersion& version : b.versions)
        if (!a.version(version.id))
            lines.add(QStringLiteral("version %1 removed").arg(version.id), QStringLiteral("version"));
    for (const PaperVersion& version : a.versions) {
        const PaperVersion* was = b.version(version.id);
        if (!was)
            continue;
        const QString prefix = several ? version.id + QLatin1Char(' ') : QString();
        lines.field(prefix + QStringLiteral("kind"), was->kind, version.kind);
        QStringList names;
        for (const NamedField& field : version.fields)
            names << field.name;
        for (const NamedField& field : was->fields)
            if (!names.contains(field.name))
                names << field.name;
        for (const QString& name : names)
            paperField(lines, prefix + name, versionField(was, name), versionField(&version, name));
        lines.field(prefix + QStringLiteral("pdf"), was->pdf, version.pdf);
        lines.text(prefix + QStringLiteral("differences"), was->differences, version.differences);
    }

    QStringList readingsBefore, readingsAfter, renumberingsBefore, renumberingsAfter;
    for (const ReadingEntry& reading : b.readings)
        readingsBefore << readingText(reading);
    for (const ReadingEntry& reading : a.readings)
        readingsAfter << readingText(reading);
    for (const Renumbering& renumbering : b.renumberings)
        renumberingsBefore << renumberingText(renumbering);
    for (const Renumbering& renumbering : a.renumberings)
        renumberingsAfter << renumberingText(renumbering);
    itemLines(lines, QStringLiteral("reading"), readingsBefore, readingsAfter);
    itemLines(lines, QStringLiteral("renumbering"), renumberingsBefore, renumberingsAfter);
}

void stubLines(Lines& lines, const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    const StubFile* old = fileAt(before.stubs, path);
    const StubFile* now = fileAt(after.stubs, path);
    if (!now) {
        if (old)
            deletedLines(lines, old->tag);
        else
            plainFile(lines, change);
        return;
    }
    if (!now->reading.parsed) {
        lines.add(QStringLiteral("the stub can't be read; the Problems panel says why"), QStringLiteral("stub"), true);
        return;
    }
    const auto label = [&](const QString& id) { return named(before, after, id); };
    const Stub& a = now->reading.document;
    if (!old || !old->reading.parsed) {
        lines.add(QStringLiteral("new %1 %2").arg(a.kind, label(now->tag)), QStringLiteral("new"));
        return;
    }
    const Stub& b = old->reading.document;
    const auto same = [](const QString& item) { return item; };
    lines.field(QStringLiteral("id"), b.id, a.id);
    lines.field(QStringLiteral("kind"), b.kind, a.kind);
    lines.text(QStringLiteral("title"), b.title, a.title);
    lines.text(QStringLiteral("gist"), b.gist, a.gist);
    lines.add(listChange(QStringLiteral("stated using"), b.statedUsing, a.statedUsing, label), QStringLiteral("stated using"));
    lines.add(listChange(QStringLiteral("paper premises"), b.paperPremises, a.paperPremises, label), QStringLiteral("paper premises"));
    lines.add(listChange(QStringLiteral("aliases"), b.aliases, a.aliases, same), QStringLiteral("aliases"));
    lines.field(QStringLiteral("extracted from"), b.extractedFrom, a.extractedFrom);
    lines.field(QStringLiteral("extracted by"), checker(b.extractedBy), checker(a.extractedBy));
    lines.field(QStringLiteral("created"), dateText(b.created), dateText(a.created));
    const QString what = QStringLiteral("locator");
    for (const Locator& locator : a.locators) {
        const Locator* was = b.locator(locator.version);
        if (!was) {
            lines.add(QStringLiteral("locator added: %1 at %2").arg(locator.version, quoted(locator.at)), what);
            continue;
        }
        if (was->at != locator.at)
            lines.add(fromTo(locator.version + QStringLiteral(" at"), quoted(was->at), quoted(locator.at)), what);
        if (was->checkedAgainstPdf != locator.checkedAgainstPdf)
            lines.add(locator.checkedAgainstPdf.isValid()
                          ? QStringLiteral("%1 checked against the PDF %2").arg(locator.version, dateText(locator.checkedAgainstPdf))
                          : QStringLiteral("%1 no longer checked against the PDF").arg(locator.version),
                      what);
    }
    for (const Locator& locator : b.locators)
        if (!a.locator(locator.version))
            lines.add(QStringLiteral("locator removed: %1 at %2").arg(locator.version, quoted(locator.at)), what);
}

/// The parts of a path.
QStringList partsOf(const QString& path)
{
    return path.split(QLatin1Char('/'));
}

/// The proof ID a file in `Proofs/<tag>/` belongs to (`4KX7.a` for `4KX7.a.checks.toml`), or empty.
QString proofIdOf(const QString& path)
{
    const QStringList parts = partsOf(path);
    if (parts.size() != 3 || parts[0] != u"Proofs")
        return {};
    QString name = parts[2];
    for (const char* suffix : {".verbose.tex", ".checks.toml", ".tex"}) {
        if (name.endsWith(QLatin1String(suffix))) {
            name.chop(static_cast<qsizetype>(qstrlen(suffix)));
            return name;
        }
    }
    return {};
}

/// The summary of an entry's details: the words of the main file's lines, then each other file's
/// words after its ID once (*"text, 4KX7.a premises, check"*).
QString summaryOf(const std::vector<FileChanges>& details)
{
    QStringList words;
    QString lastPrefix;
    QSet<QString> seen;
    for (const FileChanges& file : details) {
        const QString prefix = file.proofId.isEmpty() && file.label.startsWith(u"Stub ") ? file.label.mid(5) : file.proofId;
        for (const ChangeLine& line : file.lines) {
            if (line.what.isEmpty())
                continue;
            const QString key = prefix + QLatin1Char('\n') + line.what;
            if (seen.contains(key))
                continue;
            seen.insert(key);
            if (!prefix.isEmpty() && prefix != lastPrefix) {
                words << prefix + QLatin1Char(' ') + line.what;
                lastPrefix = prefix;
            } else {
                words << line.what;
            }
        }
    }
    return words.join(QStringLiteral(", "));
}

} // namespace

QString reviewGroupName(ReviewGroup group)
{
    switch (group) {
    case ReviewGroup::Statements:
        return QStringLiteral("Statements and proofs");
    case ReviewGroup::Papers:
        return QStringLiteral("Papers");
    case ReviewGroup::Markdown:
        return QStringLiteral("Markdown");
    case ReviewGroup::ProjectFiles:
        return QStringLiteral("Project files");
    case ReviewGroup::Other:
        return QStringLiteral("Other files");
    }
    return {};
}

ReviewGroup reviewGroupOf(const QString& path, QString& key)
{
    const QStringList parts = partsOf(path);
    if (parts.size() == 2 && parts[0] == u"Statements" && parts[1].endsWith(u".tex")) {
        key = parts[1].chopped(4);
        return ReviewGroup::Statements;
    }
    if (parts.size() >= 3 && parts[0] == u"Proofs") {
        key = parts[1];
        return ReviewGroup::Statements;
    }
    if (parts.size() == 3 && parts[0] == u"Papers" && parts[2] == u"paper.toml") {
        key = parts[1];
        return ReviewGroup::Papers;
    }
    if (parts.size() == 4 && parts[0] == u"Papers" && parts[2] == u"Statements" && parts[3].endsWith(u".toml")) {
        key = parts[1];
        return ReviewGroup::Papers;
    }
    key = path;
    if (path == u"loom.toml" || (parts[0] == u"TeX" && parts.size() > 1) || path == u"CLAUDE.md" || path == u".gitignore"
        || path == u".gitattributes")
        return ReviewGroup::ProjectFiles;
    if (path.endsWith(u".md", Qt::CaseInsensitive))
        return ReviewGroup::Markdown;
    return ReviewGroup::Other;
}

FileChanges fileChanges(const ProjectContents& before, const ProjectContents& after, const QString& path, FileChange change)
{
    FileChanges file;
    file.path = path;
    file.change = change;
    Lines lines(file.lines);
    const QStringList parts = partsOf(path);
    QString key;
    const ReviewGroup group = reviewGroupOf(path, key);
    if (group == ReviewGroup::Statements && parts[0] == u"Statements") {
        file.label = QStringLiteral("Statement");
        statementLines(lines, before, after, path, change);
    } else if (group == ReviewGroup::Statements && !(file.proofId = proofIdOf(path)).isEmpty()) {
        if (path.endsWith(u".verbose.tex")) {
            file.label = QStringLiteral("Verbose version of ") + file.proofId;
            verboseLines(lines, before, after, path, change);
        } else if (path.endsWith(u".checks.toml")) {
            file.label = QStringLiteral("Checks of ") + file.proofId;
            checksLines(lines, before, after, path, change);
        } else {
            file.label = QStringLiteral("Proof ") + file.proofId;
            proofLines(lines, before, after, path, change);
        }
    } else if (group == ReviewGroup::Papers && parts.size() == 3) {
        file.label = QStringLiteral("paper.toml");
        paperLines(lines, before, after, path, change);
    } else if (group == ReviewGroup::Papers) {
        file.label = QStringLiteral("Stub ") + parts[3].chopped(5);
        stubLines(lines, before, after, path, change);
    } else {
        file.label = path;
        plainFile(lines, change);
    }
    return file;
}

ReviewList reviewList(const std::vector<ChangedFile>& files, const ProjectContents& before, const ProjectContents& after)
{
    ReviewList list;
    list.statusChanges = statusChanges(before, after);

    std::map<std::pair<ReviewGroup, QString>, ReviewEntry> entries;
    for (const ChangedFile& changed : files) {
        QString key;
        const ReviewGroup group = reviewGroupOf(changed.path, key);
        ReviewEntry& entry = entries[{group, key}];
        entry.group = group;
        entry.key = key;
        entry.files.push_back(changed);
    }
    for (auto& [id, entry] : entries) {
        std::sort(entry.files.begin(), entry.files.end(), [](const ChangedFile& a, const ChangedFile& b) { return a.path < b.path; });
        if (entry.group != ReviewGroup::Statements && entry.group != ReviewGroup::Papers) {
            entry.name = entry.key;
            entry.change = entry.files.front().change;
            list.entries.push_back(std::move(entry));
            continue;
        }
        const QString main = entry.group == ReviewGroup::Statements ? QStringLiteral("Statements/%1.tex").arg(entry.key)
                                                                    : QStringLiteral("Papers/%1/paper.toml").arg(entry.key);
        // The main file (the statement, or paper.toml) first; then each proof's file before its
        // check history and verbose version, so a proof's lines read in that order; the rest by path.
        const auto rank = [&](const QString& path) {
            if (path == main)
                return std::make_pair(QString(), 0);
            const QString proof = proofIdOf(path);
            if (proof.isEmpty())
                return std::make_pair(path, 0);
            return std::make_pair(proof, path.endsWith(u".checks.toml") ? 2 : path.endsWith(u".verbose.tex") ? 3 : 1);
        };
        std::stable_sort(entry.files.begin(), entry.files.end(),
                         [&](const ChangedFile& a, const ChangedFile& b) { return rank(a.path) < rank(b.path); });
        for (const ChangedFile& file : entry.files) {
            if (file.path == main)
                entry.change = file.change;
            FileChanges details = fileChanges(before, after, file.path, file.change);
            if (details.lines.empty())
                details.lines.push_back({QStringLiteral("no change to what Loom reads (comments or spacing)"), {}, false});
            entry.details.push_back(std::move(details));
        }
        entry.summary = summaryOf(entry.details);
        if (entry.group == ReviewGroup::Statements) {
            entry.before = before.statement(entry.key) != nullptr;
            entry.after = after.statement(entry.key) != nullptr;
            entry.name = named(before, after, entry.key);
        } else {
            QString title;
            for (const ProjectContents* contents : {&after, &before}) {
                if (const PaperFile* paper = contents->paper(entry.key); paper && paper->reading.document.work.title) {
                    title = paper->reading.document.work.title->text();
                    break;
                }
            }
            entry.name = title.isEmpty() ? entry.key : entry.key + dot + title;
        }
        list.entries.push_back(std::move(entry));
    }
    return list;
}

} // namespace loom
