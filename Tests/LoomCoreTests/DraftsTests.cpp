// Recovery files (Specs/m2-statements-review.md, "Never losing text"; Verification, "Recovery
// files": "Written, listed and removed"). What the app does with them on opening a project (the
// three cases) is LoomAppTests'; here, what LoomCore keeps and lists.

#include "LoomCore/Drafts.h"

#include "ProjectFixture.h"

#include <QDir>
#include <QFile>
#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

using loomtest::ProjectFixture;

namespace {

/// A draft of 4KX7 written at 2026-11-05 14:02:11 Central.
loom::Draft draftOf4kx7()
{
    loom::Draft draft;
    draft.path = "Statements/4KX7.tex";
    draft.text = "+++\nid = \"4KX7\"\n+++\nHalf-typed \\frac{1}{\r\n";
    draft.base = "9f2c";
    draft.written = QDateTime(QDate(2026, 11, 5), QTime(14, 2, 11), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600));
    return draft;
}

} // namespace

TEST_CASE("A recovery file is its text and a .toml saying what it is", "[drafts]")
{
    ProjectFixture project;
    const loom::Draft draft = draftOf4kx7();
    REQUIRE(loom::writeDraft(project.root, draft).isEmpty());

    CHECK(project.read(".loom/drafts/Statements/4KX7.tex") == draft.text);
    CHECK(project.read(".loom/drafts/Statements/4KX7.tex.toml")
          == "path = \"Statements/4KX7.tex\"\nbase = \"9f2c\"\nwritten = 2026-11-05T14:02:11-06:00\n");

    const std::vector<loom::Draft> listed = loom::listDrafts(project.root);
    REQUIRE(listed.size() == 1);
    CHECK(listed[0].path == draft.path);
    CHECK(listed[0].text == draft.text);
    CHECK(listed[0].base == draft.base);
    CHECK(listed[0].written == draft.written);
    CHECK(listed[0].written.offsetFromUtc() == -6 * 3600);

    SECTION("writing again replaces it")
    {
        loom::Draft newer = draft;
        newer.text = "More text.\n";
        REQUIRE(loom::writeDraft(project.root, newer).isEmpty());
        REQUIRE(loom::listDrafts(project.root).size() == 1);
        CHECK(loom::listDrafts(project.root)[0].text == "More text.\n");
    }
    SECTION("removing it removes both files and the folders it leaves empty")
    {
        REQUIRE(loom::removeDraft(project.root, draft.path).isEmpty());
        CHECK(loom::listDrafts(project.root).empty());
        CHECK(!QFile::exists(project.root + "/.loom/drafts/Statements"));
        CHECK(loom::removeDraft(project.root, draft.path).isEmpty()); // Not there: no error.
    }
}

TEST_CASE("Listing ignores what isn't a whole draft", "[drafts]")
{
    ProjectFixture project;
    CHECK(loom::listDrafts(project.root).empty()); // No `.loom/drafts/` yet.

    // A text without its `.toml`: a crash between the two writes.
    project.write(".loom/drafts/Statements/5HN8.tex", "Text.\n");
    // A `.toml` whose text is gone.
    project.write(".loom/drafts/Statements/6BWT.tex.toml", "path = \"Statements/6BWT.tex\"\nbase = \"\"\n");
    // A `.toml` that says it is for another file than where it sits.
    project.write(".loom/drafts/Statements/7RT2.tex", "Text.\n");
    project.write(".loom/drafts/Statements/7RT2.tex.toml", "path = \"Statements/8QZ3.tex\"\nbase = \"\"\n");
    // A `.toml` that doesn't parse.
    project.write(".loom/drafts/Statements/9D3F.tex", "Text.\n");
    project.write(".loom/drafts/Statements/9D3F.tex.toml", "path = \"Statements/9D3F.tex\n");
    // And one whole draft, a form's, with no `written`.
    project.write(".loom/drafts/new/statement.txt", "Body.\n");
    project.write(".loom/drafts/new/statement.txt.toml", "path = \"new/statement.txt\"\nbase = \"\"\n");

    const std::vector<loom::Draft> listed = loom::listDrafts(project.root);
    REQUIRE(listed.size() == 1);
    CHECK(listed[0].path == "new/statement.txt");
    CHECK(listed[0].base.isEmpty());
    CHECK(!listed[0].written.isValid());
}

TEST_CASE("Drafts are only for project paths outside .loom", "[drafts]")
{
    ProjectFixture project;
    loom::Draft draft = draftOf4kx7();
    for (const char* path : {"../outside.tex", "/abs.tex", ".loom/drafts/x.tex", "Statements\\4KX7.tex", ""}) {
        draft.path = path;
        CHECK(!loom::writeDraft(project.root, draft).isEmpty());
    }
    CHECK(!QDir(project.root + "/.loom/drafts").exists());
}
