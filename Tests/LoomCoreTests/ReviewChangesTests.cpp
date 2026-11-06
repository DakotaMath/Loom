// Review's list and the front-matter changes in plain words (Specs/m2-statements-review.md, "The
// Review tab" and "Rules: Front-matter changes in plain words"; Verification: "Plain-word changes
// and status changes: every row of both rules tables, on the `review` overlay"). The overlay is
// compared against its commit as Review compares it; the rows it doesn't reach are edits made
// here to a copy of the demo, each read before and after.

#include "LoomCore/ReviewChanges.h"

#include "LoomCore/GitOperations.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/Revision.h"

#include "DemoProject.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <catch2/catch_test_macros.hpp>

using loom::ChangedFile;
using loom::FileChange;
using loom::ProjectContents;
using loom::ReviewGroup;
using loomtest::DemoProject;

namespace {

/// Reads a project's contents, failing the test if it can't.
ProjectContents contentsOf(const QString& root)
{
    loom::ContentsLoading loading = loom::loadContents(root);
    REQUIRE(loading.ok);
    return std::move(loading.contents);
}

/// Replaces `from` by `to` in a file of the demo, failing the test if `from` isn't there.
void edit(const DemoProject& demo, const QString& path, const QByteArray& from, const QByteArray& to)
{
    QFile file(demo.root() + u'/' + path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    QByteArray bytes = file.readAll();
    file.close();
    INFO(path.toStdString() << " should contain " << from.toStdString());
    REQUIRE(bytes.contains(from));
    bytes.replace(from, to);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(bytes) == bytes.size());
}

/// Writes a file of the demo, creating its folder.
void write(const DemoProject& demo, const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(demo.root() + u'/' + path).absolutePath());
    QFile file(demo.root() + u'/' + path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(bytes) == bytes.size());
}

/// The lines' texts.
QStringList texts(const loom::FileChanges& file)
{
    QStringList out;
    for (const loom::ChangeLine& line : file.lines)
        out << line.text;
    return out;
}

/// The texts of the lines one file's change gives between `before` and the demo now.
QStringList linesOf(const ProjectContents& before, const DemoProject& demo, const QString& path, FileChange change = FileChange::Changed)
{
    return texts(loom::fileChanges(before, contentsOf(demo.root()), path, change));
}

/// The entry with `key`, or null.
const loom::ReviewEntry* entryOf(const loom::ReviewList& list, const QString& key)
{
    for (const loom::ReviewEntry& entry : list.entries)
        if (entry.key == key)
            return &entry;
    return nullptr;
}

/// The status changes as `ID: before → after mark` strings.
QStringList statusLines(const loom::ReviewList& list)
{
    QStringList out;
    for (const loom::StatusChange& change : list.statusChanges) {
        const QString mark = change.direction == loom::StatusDirection::Raise  ? QStringLiteral(" up")
                             : change.direction == loom::StatusDirection::Drop ? QStringLiteral(" down")
                                                                               : QString();
        out << QStringLiteral("%1: %2>%3%4").arg(change.id, loom::statusName(change.before), loom::statusName(change.after), mark);
    }
    return out;
}

} // namespace

TEST_CASE("Review groups changed files as the spec's table says", "[review]")
{
    QString key;
    CHECK(loom::reviewGroupOf(QStringLiteral("Statements/4KX7.tex"), key) == ReviewGroup::Statements);
    CHECK(key == u"4KX7");
    CHECK(loom::reviewGroupOf(QStringLiteral("Proofs/4KX7/4KX7.a.checks.toml"), key) == ReviewGroup::Statements);
    CHECK(key == u"4KX7");
    CHECK(loom::reviewGroupOf(QStringLiteral("Papers/SmithJones24/paper.toml"), key) == ReviewGroup::Papers);
    CHECK(key == u"SmithJones24");
    CHECK(loom::reviewGroupOf(QStringLiteral("Papers/SmithJones24/Statements/7RT2.toml"), key) == ReviewGroup::Papers);
    CHECK(key == u"SmithJones24");
    CHECK(loom::reviewGroupOf(QStringLiteral("Papers/SmithJones24/notes.md"), key) == ReviewGroup::Markdown);
    CHECK(key == u"Papers/SmithJones24/notes.md");
    CHECK(loom::reviewGroupOf(QStringLiteral("Notes/plan.md"), key) == ReviewGroup::Markdown);
    for (const char* path : {"loom.toml", "TeX/macros.tex", "CLAUDE.md", ".gitignore", ".gitattributes"}) {
        INFO(path);
        CHECK(loom::reviewGroupOf(QString::fromLatin1(path), key) == ReviewGroup::ProjectFiles);
    }
    CHECK(loom::reviewGroupOf(QStringLiteral("Papers/SmithJones24/local/paper.pdf"), key) == ReviewGroup::Other);
    CHECK(loom::reviewGroupOf(QStringLiteral("Records/2026-11/demo-0001.md"), key) == ReviewGroup::Markdown);
    CHECK(loom::reviewGroupOf(QStringLiteral("figure.png"), key) == ReviewGroup::Other);
    CHECK(loom::reviewGroupName(ReviewGroup::Statements) == u"Statements and proofs");
}

TEST_CASE("Review lists the review overlay against HEAD", "[review]")
{
    const DemoProject demo(QStringLiteral("review"));
    REQUIRE(demo.ok());
    const loom::GitState state = loom::readGitState(demo.git(), demo.root());
    REQUIRE(state.ok);
    const loom::RevisionTree tree = loom::readRevision(demo.root(), QStringLiteral("HEAD"), demo.git());
    REQUIRE(tree.outcome == loom::RevisionOutcome::Ready);
    const ProjectContents before = contentsOf(demo.root() + u'/' + tree.folder);
    const ProjectContents after = contentsOf(demo.root());
    const loom::ReviewList list = loom::reviewList(state.files, before, after);

    // Four entries: three statements and the Markdown note; the badge counts them.
    QStringList keys;
    for (const loom::ReviewEntry& entry : list.entries)
        keys << entry.key;
    CHECK(keys == QStringList{QStringLiteral("C4JK"), QStringLiteral("G8TV"), QStringLiteral("M7QR"), QStringLiteral("Papers/SmithJones24/notes.md")});
    CHECK(list.count() == 4);

    // A body edit to C4JK, outdating the checks of C4JK.a and D5MN.a.
    const loom::ReviewEntry* c4jk = entryOf(list, QStringLiteral("C4JK"));
    REQUIRE(c4jk);
    CHECK(c4jk->group == ReviewGroup::Statements);
    CHECK(c4jk->name == QStringLiteral(u"C4JK · Tails of tame sequences vanish"));
    CHECK(c4jk->change == FileChange::Changed);
    CHECK(c4jk->summary == u"text");
    CHECK(c4jk->before);
    CHECK(c4jk->after);
    REQUIRE(c4jk->details.size() == 1);
    CHECK(c4jk->details[0].label == u"Statement");
    CHECK(texts(c4jk->details[0]) == QStringList{QStringLiteral("text changed; outdates the checks of C4JK.a, D5MN.a")});

    // A premise added to G8TV.a and Dakota's pass added to its history.
    const loom::ReviewEntry* g8tv = entryOf(list, QStringLiteral("G8TV"));
    REQUIRE(g8tv);
    CHECK(g8tv->change == FileChange::Changed);
    CHECK(g8tv->summary == u"G8TV.a premises, check");
    REQUIRE(g8tv->details.size() == 2);
    CHECK(g8tv->details[0].label == u"Proof G8TV.a");
    CHECK(texts(g8tv->details[0]) == QStringList{QStringLiteral(u"premises: +D5MN · Tame sequences are bounded")});
    CHECK(g8tv->details[1].label == u"Checks of G8TV.a");
    CHECK(texts(g8tv->details[1]) == QStringList{QStringLiteral("check: Dakota pass (this version)")});

    // A new statement: no build at HEAD.
    const loom::ReviewEntry* m7qr = entryOf(list, QStringLiteral("M7QR"));
    REQUIRE(m7qr);
    CHECK(m7qr->change == FileChange::New);
    CHECK_FALSE(m7qr->before);
    CHECK(m7qr->after);
    CHECK(m7qr->summary == u"new");
    CHECK(texts(m7qr->details[0]) == QStringList{QStringLiteral(u"new lemma M7QR · Tails of products")});

    // The Markdown note: the raw diff only, no plain words.
    const loom::ReviewEntry* note = entryOf(list, QStringLiteral("Papers/SmithJones24/notes.md"));
    REQUIRE(note);
    CHECK(note->group == ReviewGroup::Markdown);
    CHECK(note->details.empty());
    REQUIRE(note->files.size() == 1);

    // The statuses the computation gives at HEAD and now: drops first, then the rest. G8TV stays
    // unchecked: Dakota's pass is of this version, the agent's of the one before.
    CHECK(statusLines(list) == QStringList{QStringLiteral("C4JK: proved>unchecked down"), QStringLiteral("D5MN: proved>unchecked down"),
                                           QStringLiteral("E6PQ: proved>conditional down"), QStringLiteral("M7QR: >open")});
}

TEST_CASE("A statement's front-matter changes in plain words", "[review]")
{
    const DemoProject demo;
    REQUIRE(demo.ok());
    const ProjectContents before = contentsOf(demo.root());
    const QString path = QStringLiteral("Statements/C4JK.tex");

    SECTION("title, kind, tags, stated using, aliases, origin, verified")
    {
        edit(demo, path, "kind = \"lemma\"\ntitle = \"Tails of tame sequences vanish\"\ntags = [\"tame-sequences\", \"tail-bounds\"]\n"
                         "stated_using = [\"A2DF\", \"B3GH\"]\ncreated = 2026-11-05\n",
             "kind = \"proposition\"\ntitle = \"Tails vanish\"\ntags = [\"tail-bounds\", \"energy\"]\nstated_using = [\"A2DF\"]\n"
             "aliases = [\"lem:tails\"]\norigin = \"SmithJones24\"\ncreated = 2026-11-05\nverified = 2026-11-05\n");
        CHECK(linesOf(before, demo, path)
              == QStringList{QStringLiteral(u"kind: lemma → proposition"),
                             QStringLiteral(u"title: \"Tails of tame sequences vanish\" → \"Tails vanish\""),
                             QStringLiteral(u"tags: +energy, −tame-sequences"),
                             QStringLiteral(u"stated using: −B3GH · The tail of a sequence"),
                             QStringLiteral("aliases: +lem:tails"),
                             QStringLiteral(u"origin: new → SmithJones24"),
                             QStringLiteral("front matter verified 2026-11-05")});
    }
    SECTION("superseded and refuted, set and cleared")
    {
        edit(demo, path, "created = 2026-11-05\n",
             "superseded_by = \"D5MN\"\nrefuted_by = \"G8TV.a\"\nrefuted_on = 2026-11-06\ncreated = 2026-11-05\n");
        const ProjectContents middle = contentsOf(demo.root());
        CHECK(linesOf(before, demo, path)
              == QStringList{QStringLiteral(u"superseded by D5MN · Tame sequences are bounded"),
                             QStringLiteral("refuted by G8TV.a (accepted 2026-11-06)")});
        edit(demo, path, "superseded_by = \"D5MN\"\nrefuted_by = \"G8TV.a\"\nrefuted_on = 2026-11-06\n", "");
        CHECK(linesOf(middle, demo, path) == QStringList{QStringLiteral("no longer superseded"), QStringLiteral("no longer refuted")});
    }
    SECTION("refuted, not yet accepted; an alias removed")
    {
        edit(demo, path, "created = 2026-11-05\n", "aliases = [\"lem:tails\", \"lem:vanish\"]\ncreated = 2026-11-05\n");
        const ProjectContents middle = contentsOf(demo.root());
        edit(demo, path, "aliases = [\"lem:tails\", \"lem:vanish\"]\n", "aliases = [\"lem:tails\"]\nrefuted_by = \"G8TV.a\"\n");
        CHECK(linesOf(middle, demo, path)
              == QStringList{QStringLiteral(u"aliases: \u2212lem:vanish"), QStringLiteral("refuted by G8TV.a (not yet accepted)")});
    }
    SECTION("a file that couldn't be read at the last commit")
    {
        edit(demo, path, "kind = \"lemma\"", "kind = = \"lemma\"");
        const ProjectContents broken = contentsOf(demo.root());
        edit(demo, path, "kind = = \"lemma\"", "kind = \"lemma\"");
        CHECK(linesOf(broken, demo, path) == QStringList{QStringLiteral("couldn't be read at the last commit; it can now")});
    }
    SECTION("the body, with the checks it outdates")
    {
        edit(demo, path, "tend to zero", "go to zero");
        CHECK(linesOf(before, demo, path) == QStringList{QStringLiteral("text changed; outdates the checks of C4JK.a, D5MN.a")});
    }
    SECTION("a body change that outdates no check")
    {
        edit(demo, QStringLiteral("Statements/K3AB.tex"), "+++\n", "+++\n");
        const QStringList lines = linesOf(before, demo, QStringLiteral("Statements/K3AB.tex"));
        CHECK(lines.isEmpty()); // Nothing Loom reads changed.
        QFile file(demo.root() + QStringLiteral("/Statements/K3AB.tex"));
        REQUIRE(file.open(QIODevice::Append));
        file.write("More text.\n");
        file.close();
        CHECK(linesOf(before, demo, QStringLiteral("Statements/K3AB.tex")) == QStringList{QStringLiteral("text changed")});
    }
    SECTION("deleted, with M1's warning")
    {
        REQUIRE(QFile::remove(demo.root() + u'/' + path));
        const loom::FileChanges file = loom::fileChanges(before, contentsOf(demo.root()), path, FileChange::Deleted);
        REQUIRE(file.lines.size() == 2);
        CHECK(file.lines[0].text == u"deleted");
        CHECK_FALSE(file.lines[0].warning);
        CHECK(file.lines[1].warning);
        CHECK(file.lines[1].text.startsWith(u"C4JK was in the last commit but its file is gone."));
    }
}

TEST_CASE("A proof's changes in plain words", "[review]")
{
    const DemoProject demo;
    REQUIRE(demo.ok());
    const ProjectContents before = contentsOf(demo.root());

    SECTION("premises and level")
    {
        edit(demo, QStringLiteral("Proofs/G8TV/G8TV.a.tex"), "premises = [\"C4JK\"]\nlevel = \"complete\"",
             "premises = [\"D5MN\"]\nlevel = \"sketch\"");
        CHECK(linesOf(before, demo, QStringLiteral("Proofs/G8TV/G8TV.a.tex"))
              == QStringList{QStringLiteral(u"premises: +D5MN · Tame sequences are bounded, −C4JK · Tails of tame sequences vanish"),
                             QStringLiteral(u"level: complete → sketch")});
    }
    SECTION("proves, with titles")
    {
        edit(demo, QStringLiteral("Proofs/G8TV/G8TV.a.tex"), "proves = \"G8TV\"", "proves = \"K3AB\"");
        CHECK(linesOf(before, demo, QStringLiteral("Proofs/G8TV/G8TV.a.tex"))
              == QStringList{QStringLiteral(u"proves: G8TV \u00b7 Tame sums commute with limits \u2192 K3AB \u00b7 A tail bound for sums")});
    }
    SECTION("the body of a checked proof")
    {
        edit(demo, QStringLiteral("Proofs/D5MN/D5MN.a.tex"), "+++\n", "+++\n");
        QFile file(demo.root() + QStringLiteral("/Proofs/D5MN/D5MN.a.tex"));
        REQUIRE(file.open(QIODevice::Append));
        file.write("A last remark.\n");
        file.close();
        CHECK(linesOf(before, demo, QStringLiteral("Proofs/D5MN/D5MN.a.tex")) == QStringList{QStringLiteral("text changed; outdates the checks of D5MN.a")});
    }
    SECTION("a new proof")
    {
        write(demo, QStringLiteral("Proofs/K3AB/K3AB.a.tex"),
              "+++\nid = \"K3AB.a\"\nproves = \"K3AB\"\npremises = [\"C4JK\"]\nlevel = \"sketch\"\ncreated = 2026-11-05\n+++\nBy C4JK.\n");
        CHECK(linesOf(before, demo, QStringLiteral("Proofs/K3AB/K3AB.a.tex"), FileChange::New)
              == QStringList{QStringLiteral(u"new proof K3AB.a (sketch, from C4JK · Tails of tame sequences vanish)")});
    }
    SECTION("a deleted proof")
    {
        REQUIRE(QFile::remove(demo.root() + QStringLiteral("/Proofs/H9WX/H9WX.a.tex")));
        const QStringList lines = linesOf(before, demo, QStringLiteral("Proofs/H9WX/H9WX.a.tex"), FileChange::Deleted);
        REQUIRE(lines.size() == 2);
        CHECK(lines[0] == u"deleted");
        CHECK(lines[1].startsWith(u"H9WX.a was in the last commit"));
    }
}

TEST_CASE("Checks and verbose versions in plain words", "[review]")
{
    const DemoProject demo;
    REQUIRE(demo.ok());
    const ProjectContents before = contentsOf(demo.root());

    SECTION("checks added, for this version or an older one")
    {
        const QString path = QStringLiteral("Proofs/G8TV/G8TV.a.checks.toml");
        const QString fingerprint = before.proof(QStringLiteral("G8TV.a")) ? loom::proofChecking(before, QStringLiteral("G8TV.a")).fingerprint : QString();
        REQUIRE_FALSE(fingerprint.isEmpty());
        QFile file(demo.root() + u'/' + path);
        REQUIRE(file.open(QIODevice::Append));
        file.write(QStringLiteral("\n[[checks]]\nby = \"dakota\"\ndate = 2026-11-05T11:00:00-06:00\nfingerprint = \"%1\"\nverdict = \"pass\"\n")
                       .arg(fingerprint).toUtf8());
        file.write("\n[[checks]]\nby = \"agent:claude-opus-5-5\"\ndate = 2026-11-05T12:00:00-06:00\nfingerprint = \"000000000000\"\n"
                   "verdict = \"needs-changes\"\nchanges = \"\"\"\nStep 3 uses the bound twice.\nThe second use needs N large.\"\"\"\n");
        file.close();
        CHECK(linesOf(before, demo, path)
              == QStringList{QStringLiteral("check: Dakota pass (this version)"),
                             QStringLiteral("check: agent claude-opus-5-5 needs-changes: \"Step 3 uses the bound twice.\" (an older version)")});
    }
    SECTION("a committed check changed")
    {
        const QString path = QStringLiteral("Proofs/G8TV/G8TV.a.checks.toml");
        edit(demo, path, "verdict = \"pass\"", "verdict = \"fail\"");
        const loom::FileChanges file = loom::fileChanges(before, contentsOf(demo.root()), path, FileChange::Changed);
        REQUIRE(file.lines.size() == 2);
        CHECK(file.lines[0].text == u"check changed: agent claude-opus-5-5, 2026-11-05 10:00");
        CHECK(file.lines[0].warning);
        CHECK(file.lines[1].text == u"check: agent claude-opus-5-5 fail (this version)");
        CHECK(file.proofId == u"G8TV.a");
    }
    SECTION("a committed check removed")
    {
        const QString path = QStringLiteral("Proofs/G8TV/G8TV.a.checks.toml");
        write(demo, path, "");
        const loom::FileChanges file = loom::fileChanges(before, contentsOf(demo.root()), path, FileChange::Changed);
        REQUIRE(file.lines.size() == 1);
        CHECK(file.lines[0].text == u"check changed: agent claude-opus-5-5, 2026-11-05 10:00");
        CHECK(file.lines[0].warning);
    }
    SECTION("a check history deleted, and one new")
    {
        const QString path = QStringLiteral("Proofs/G8TV/G8TV.a.checks.toml");
        REQUIRE(QFile::remove(demo.root() + u'/' + path));
        const ProjectContents without = contentsOf(demo.root());
        const loom::FileChanges gone = loom::fileChanges(before, without, path, FileChange::Deleted);
        CHECK(texts(gone) == QStringList{QStringLiteral("deleted"), QStringLiteral("check changed: agent claude-opus-5-5, 2026-11-05 10:00")});
        CHECK(gone.lines[0].warning);
        CHECK(texts(loom::fileChanges(without, before, path, FileChange::New))
              == QStringList{QStringLiteral("check: agent claude-opus-5-5 pass (this version)")});
    }
    SECTION("a verbose version marked up to date, changed, removed and added")
    {
        const QString path = QStringLiteral("Proofs/E6PQ/E6PQ.a.verbose.tex");
        edit(demo, path, "fingerprint = \"35deec0037c1\"", "fingerprint = \"111111111111\"");
        CHECK(linesOf(before, demo, path) == QStringList{QStringLiteral("verbose version marked up to date")});
        edit(demo, path, "Write $M", "Let $M");
        CHECK(linesOf(before, demo, path) == QStringList{QStringLiteral("verbose version changed")});
        const ProjectContents with = contentsOf(demo.root());
        REQUIRE(QFile::remove(demo.root() + u'/' + path));
        const ProjectContents without = contentsOf(demo.root());
        CHECK(texts(loom::fileChanges(with, without, path, FileChange::Deleted)) == QStringList{QStringLiteral("verbose version removed")});
        CHECK(texts(loom::fileChanges(without, with, path, FileChange::New)) == QStringList{QStringLiteral("verbose version added")});
    }
}

TEST_CASE("Papers and stubs in plain words", "[review]")
{
    const DemoProject demo;
    REQUIRE(demo.ok());
    const ProjectContents before = contentsOf(demo.root());

    SECTION("paper.toml: a field, a field's verified, a version and a reading")
    {
        const QString path = QStringLiteral("Papers/SmithJones24/paper.toml");
        edit(demo, path, "journal = { value = \"Journal of Example Analysis\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03 }",
             "journal = { value = \"J. Example Anal.\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03 }");
        edit(demo, path, "title = { value = \"Summable sequences and their products\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03 }",
             "title = { value = \"Summable sequences and their products\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = false }");
        QFile file(demo.root() + u'/' + path);
        REQUIRE(file.open(QIODevice::Append));
        file.write("\n[[reading]]\nversion = \"published\"\nlevel = \"read\"\nwhat = \"Section 2\"\ndate = 2026-11-05\n");
        file.close();
        CHECK(linesOf(before, demo, path)
              == QStringList{QStringLiteral("title no longer verified"),
                             QStringLiteral(u"published journal: \"Journal of Example Analysis\" → \"J. Example Anal.\""),
                             QStringLiteral("reading added: read published, 2026-11-05 (Section 2)")});
    }
    SECTION("a stub: a locator's place and its PDF check")
    {
        const QString path = QStringLiteral("Papers/SmithJones24/Statements/7RT2.toml");
        edit(demo, path, "published = { at = \"Theorem 1.2\", checked_against_pdf = 2026-11-05 }",
             "published = { at = \"Theorem 1.3\", checked_against_pdf = false }");
        CHECK(linesOf(before, demo, path)
              == QStringList{QStringLiteral(u"published at: \"Theorem 1.2\" → \"Theorem 1.3\""),
                             QStringLiteral("published no longer checked against the PDF")});
    }
    SECTION("a deleted stub, with M1's warning")
    {
        const QString path = QStringLiteral("Papers/SmithJones24/Statements/3WQ6.toml");
        REQUIRE(QFile::remove(demo.root() + u'/' + path));
        const loom::FileChanges file = loom::fileChanges(before, contentsOf(demo.root()), path, FileChange::Deleted);
        REQUIRE(file.lines.size() == 2);
        CHECK(file.lines[0].text == u"deleted");
        CHECK(file.lines[1].warning);
        CHECK(file.lines[1].text.startsWith(u"3WQ6 was in the last commit"));
    }
    SECTION("the paper's entry groups paper.toml and its stubs")
    {
        edit(demo, QStringLiteral("Papers/SmithJones24/Statements/7RT2.toml"), "checked_against_pdf = 2026-11-05", "checked_against_pdf = false");
        const std::vector<ChangedFile> files{{QStringLiteral("Papers/SmithJones24/Statements/7RT2.toml"), FileChange::Changed, false}};
        const loom::ReviewList list = loom::reviewList(files, before, contentsOf(demo.root()));
        REQUIRE(list.entries.size() == 1);
        CHECK(list.entries[0].group == ReviewGroup::Papers);
        CHECK(list.entries[0].name == QStringLiteral(u"SmithJones24 · Summable sequences and their products"));
        CHECK(list.entries[0].summary == u"7RT2 locator");
        // 7RT2 is no longer cited: a drop from cited to extracted, and E6PQ, resting on it, drops too.
        CHECK(statusLines(list) == QStringList{QStringLiteral("7RT2: cited>extracted down"), QStringLiteral("E6PQ: proved>conditional down")});
    }
}

TEST_CASE("Review with no commit yet lists everything as new", "[review]")
{
    const DemoProject demo;
    REQUIRE(demo.ok());
    const ProjectContents after = contentsOf(demo.root());
    const std::vector<ChangedFile> files{{QStringLiteral("Statements/C4JK.tex"), FileChange::New, true},
                                         {QStringLiteral("loom.toml"), FileChange::New, true}};
    const loom::ReviewList list = loom::reviewList(files, ProjectContents(), after);
    REQUIRE(list.entries.size() == 2);
    CHECK(list.entries[0].key == u"C4JK");
    CHECK(list.entries[0].change == FileChange::New);
    CHECK_FALSE(list.entries[0].before);
    CHECK(list.entries[1].group == ReviewGroup::ProjectFiles);
    CHECK(list.entries[1].details.empty());
}
