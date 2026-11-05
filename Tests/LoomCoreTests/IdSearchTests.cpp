// Tests for the ID search and reading IDs in LaTeX (Specs/m2-statements-review.md, "IDs in the
// editor"; Verification, "ID search": "Ranking, prefix and accent matching, what each kind of field
// offers, aliases resolved to tags").

#include "PaperFixture.h"
#include "ProjectFixture.h"
#include "TestSupport.h"

#include "LoomCore/IdSearch.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/Status.h"

#include <catch2/catch_test_macros.hpp>

using namespace loom;
using namespace loomtest;

namespace {

/// A small project with something for each rule: titles, tags, aliases, accents in a body, a
/// proof, a `\label`, a paper with an old key, and two stubs (one of a kind without a status).
struct SearchProject : ProjectFixture {
    ProjectContents contents;
    ProjectGraph graph;
    IdIndex index;

    SearchProject()
    {
        writeStatement("4KX7", "theorem", "Energy decay for the model system", "We use the Hölder inequality.\n",
                       "tags = [\"model-system\"]\naliases = [\"thm:energy-decay\"]\n");
        writeStatement("5HN8", "lemma", "A uniform bound for the energy", "Bound.\n");
        writeStatement("9D3F", "lemma", "Decay of the tail", "As in \\paperref{7RT2}, the tail decays: \\label{eq:tail} $x$.\n");
        writeStatement("2M9Q", "definition", "Weak solution", "The energy of a solution.\n", "tags = [\"model-system\"]\n");
        writeStatement("6BWT", "lemma", "Model estimate", "Estimate.\n");
        proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[\"5HN8\", \"9D3F\"]");
        write(QStringLiteral("Papers/SmithJones24/paper.toml"), smithJones24());
        write(QStringLiteral("Papers/SmithJones24/Statements/7RT2.toml"),
              "id = \"7RT2\"\nkind = \"theorem\"\ngist = \"A result\"\nextracted_from = \"published\"\nextracted_by = \"dakota\"\n"
              "created = 2026-11-03\n\n[locators]\npublished = { at = \"Theorem 1.2\", checked_against_pdf = 2026-11-03 }\n");
        write(QStringLiteral("Papers/SmithJones24/Statements/3WQ6.toml"),
              "id = \"3WQ6\"\nkind = \"definition\"\ngist = \"Summable sequences\"\nextracted_from = \"arxiv-v1\"\n"
              "extracted_by = \"dakota\"\ncreated = 2026-11-03\n\n[locators]\narxiv-v1 = { at = \"Definition 2.1\", "
              "checked_against_pdf = false }\n");
        ContentsLoading loading = loadContents(root);
        REQUIRE(loading.ok);
        contents = std::move(loading.contents);
        graph = buildGraph(contents);
        index = IdIndex(contents, graph);
    }

    /// Writes a statement; `extra` lines go after `title`.
    void writeStatement(const QString& tag, const QByteArray& kind, const QByteArray& title, const QByteArray& body,
                        const QByteArray& extra = {}) const
    {
        write(QStringLiteral("Statements/%1.tex").arg(tag), "+++\nid = \"" + tag.toUtf8() + "\"\nkind = \"" + kind + "\"\ntitle = \""
                                                                 + title + "\"\n" + extra + "created = 2026-11-03\n+++\n" + body);
    }

    /// The IDs a search finds, best first.
    QStringList find(const QString& query, IdField field = IdField::Ref, const QString& self = {}, const QStringList& recent = {}) const
    {
        QStringList ids;
        for (const IdMatch& match : index.search(query, field, self, recent))
            ids << match.entry->id;
        return ids;
    }
};

} // namespace

TEST_CASE("Ranking: an exact tag or alias, then the title, then tags, then the body; ties by recently opened, then tag")
{
    const SearchProject p;
    // Title matches before a body match, ties in tag order, or most recently opened first.
    CHECK(p.find(QStringLiteral("energy")) == QStringList{"4KX7", "5HN8", "2M9Q"});
    CHECK(p.find(QStringLiteral("energy"), IdField::Ref, {}, {"5HN8", "2M9Q"}) == QStringList{"5HN8", "4KX7", "2M9Q"});
    // Title before tags.
    CHECK(p.find(QStringLiteral("model")) == QStringList{"4KX7", "6BWT", "2M9Q"});
    // An exact alias or tag (as typed tags are read) first; then what its ID starts.
    CHECK(p.find(QStringLiteral("thm:energy-decay")) == QStringList{"4KX7"});
    CHECK(p.find(QStringLiteral("4kx7")) == QStringList{"4KX7", "4KX7.a"});
    CHECK(p.find(QStringLiteral("decay")) == QStringList{"4KX7", "9D3F"});
    // An exact match outranks a recently opened one.
    CHECK(p.find(QStringLiteral("9d3f"), IdField::Ref, {}, {"4KX7"}).front() == QStringLiteral("9D3F"));

    const std::vector<IdMatch> matches = p.index.search(QStringLiteral("energy"), IdField::Ref);
    REQUIRE(matches.size() == 3);
    CHECK(matches[0].tier == 1);
    CHECK(matches[2].tier == 3);
    CHECK(p.index.search(QStringLiteral("4KX7"), IdField::Ref).front().tier == 0);
    CHECK(p.index.search(QStringLiteral("model"), IdField::Ref)[2].tier == 2);
}

TEST_CASE("Words match as prefixes of words, ignoring case and accents, and every word must match")
{
    const SearchProject p;
    CHECK(p.find(QStringLiteral("holder")) == QStringList{"4KX7"});
    CHECK(p.find(QStringLiteral("HÖL")) == QStringList{"4KX7"});
    CHECK(p.find(QStringLiteral("older")).isEmpty());          // Starts of words only.
    CHECK(p.find(QStringLiteral("energy holder")) == QStringList{"4KX7"});
    CHECK(p.find(QStringLiteral("energy tail")).isEmpty());    // Every word.
    CHECK(p.find(QStringLiteral("unif bou")) == QStringList{"5HN8"});
    CHECK(foldForSearch(QStringLiteral("Hölder Ünïcode")) == QStringLiteral("holder unicode"));
    CHECK(searchWords(QStringLiteral("thm:energy-decay")) == QStringList{"thm", "energy", "decay"});
    CHECK(matchesEveryWord(QStringLiteral("dec MOD"), searchWords(QStringLiteral("Energy decay for the model system"))));
    CHECK(matchesEveryWord(QString(), {}));
    CHECK_FALSE(matchesEveryWord(QStringLiteral("x"), {}));
}

TEST_CASE("What each field offers")
{
    const SearchProject p;
    // \ref and Go to: statements, proofs and stubs, in tag order for an empty query.
    CHECK(p.find(QString()) == QStringList{"2M9Q", "3WQ6", "4KX7", "4KX7.a", "5HN8", "6BWT", "7RT2", "9D3F"});
    // \paperref: stubs only.
    CHECK(p.find(QString(), IdField::PaperRef) == QStringList{"3WQ6", "7RT2"});
    // Premises: provable statements and stubs, not the statement itself.
    CHECK(p.find(QString(), IdField::Premises, QStringLiteral("4KX7")) == QStringList{"5HN8", "6BWT", "7RT2", "9D3F"});
    // Stated using: statements and stubs, not itself.
    CHECK(p.find(QString(), IdField::StatedUsing, QStringLiteral("4KX7"))
          == QStringList{"2M9Q", "3WQ6", "5HN8", "6BWT", "7RT2", "9D3F"});
    // Superseded by: statements, not itself.
    CHECK(p.find(QString(), IdField::SupersededBy, QStringLiteral("4KX7")) == QStringList{"2M9Q", "5HN8", "6BWT", "9D3F"});
    // \cite: paper keys, found by key, old key, authors and title.
    CHECK(p.find(QString(), IdField::Cite) == QStringList{"SmithJones24"});
    CHECK(p.find(QStringLiteral("jones"), IdField::Cite) == QStringList{"SmithJones24"});
    CHECK(p.find(QStringLiteral("parabolic"), IdField::Cite) == QStringList{"SmithJones24"});
    CHECK(p.find(QStringLiteral("energy"), IdField::Cite) == QStringList{"SmithJones24"});
    // A stub is found by its paper's key and locator.
    CHECK(p.find(QStringLiteral("smithjones theorem"), IdField::PaperRef) == QStringList{"7RT2"});

    CHECK(fieldOfCommand(QStringLiteral("ref")) == IdField::Ref);
    CHECK(fieldOfCommand(QStringLiteral("paperref")) == IdField::PaperRef);
    CHECK(fieldOfCommand(QStringLiteral("cite")) == IdField::Cite);
}

TEST_CASE("Aliases and old keys are found, and the result is the tag or current key")
{
    const SearchProject p;
    const std::vector<IdMatch> byAlias = p.index.search(QStringLiteral("thm:energy"), IdField::Ref);
    REQUIRE(byAlias.size() == 1);
    CHECK(byAlias[0].entry->id == QStringLiteral("4KX7"));
    const std::vector<IdMatch> byOldKey = p.index.search(QStringLiteral("SmithJones23"), IdField::Cite);
    REQUIRE(byOldKey.size() == 1);
    CHECK(byOldKey[0].entry->id == QStringLiteral("SmithJones24"));
    CHECK(byOldKey[0].tier == 0);

    REQUIRE(p.index.lookup(QStringLiteral("thm:energy-decay")));
    CHECK(p.index.lookup(QStringLiteral("thm:energy-decay"))->id == QStringLiteral("4KX7"));
    CHECK(p.index.lookup(QStringLiteral("4kx7.A"))->id == QStringLiteral("4KX7.a"));
    CHECK(p.index.lookup(QStringLiteral("smithjones23"))->id == QStringLiteral("SmithJones24"));
    CHECK(p.index.lookup(QStringLiteral("ZZZZ")) == nullptr);
}

TEST_CASE("Each entry's ID and title, as the picker and the annotations show them")
{
    const SearchProject p;
    CHECK(p.index.entry(QStringLiteral("4KX7"))->label() == QStringLiteral("4KX7 · Energy decay for the model system"));
    CHECK(p.index.entry(QStringLiteral("4KX7"))->kindName == QStringLiteral("theorem"));
    CHECK(p.index.entry(QStringLiteral("4KX7.a"))->label() == QStringLiteral("4KX7.a · proof of Energy decay for the model system"));
    CHECK(p.index.entry(QStringLiteral("4KX7.a"))->kindName == QStringLiteral("complete"));
    CHECK(p.index.entry(QStringLiteral("4KX7.a"))->statement == QStringLiteral("4KX7"));
    CHECK(p.index.entry(QStringLiteral("7RT2"))->label() == QStringLiteral("7RT2 · SmithJones24 Theorem 1.2 · A result"));
    CHECK(p.index.entry(QStringLiteral("SmithJones24"))->label()
          == QStringLiteral("SmithJones24 · Smith · Energy decay for a model parabolic system"));
    CHECK(p.index.entry(QStringLiteral("SmithJones23")) == nullptr); // Old keys aren't entries.
}

TEST_CASE("What \\ref, \\paperref and \\cite name")
{
    const SearchProject p;
    const auto resolved = [&](const char* command, const char* name) { return p.index.resolve(QString::fromLatin1(command), QString::fromUtf8(name)); };

    IdResolution r = resolved("ref", "4KX7");
    CHECK((r.found && r.kind == IdKind::Statement && r.id == u"4KX7" && !r.alias));
    r = resolved("ref", "thm:energy-decay");
    CHECK((r.found && r.kind == IdKind::Statement && r.id == u"4KX7" && r.alias));
    r = resolved("ref", "4KX7.a");
    CHECK((r.found && r.kind == IdKind::Proof && r.id == u"4KX7.a"));
    r = resolved("ref", "7RT2");
    CHECK((r.found && r.kind == IdKind::Stub));
    r = resolved("ref", "eq:tail");
    CHECK((r.found && r.kind == IdKind::Label));
    CHECK_FALSE(resolved("ref", "4kx7").found); // Labels are exact.
    CHECK_FALSE(resolved("ref", "ZZZZ").found);
    CHECK_FALSE(resolved("ref", "SmithJones24").found);

    r = resolved("paperref", "7RT2");
    CHECK((r.found && r.kind == IdKind::Stub && r.id == u"7RT2"));
    CHECK_FALSE(resolved("paperref", "4KX7").found);
    CHECK_FALSE(resolved("paperref", "eq:tail").found);

    r = resolved("cite", "SmithJones24");
    CHECK((r.found && r.kind == IdKind::Paper && r.id == u"SmithJones24" && !r.alias));
    r = resolved("cite", "smithjones24");
    CHECK((r.found && r.id == u"SmithJones24" && r.alias));
    r = resolved("cite", "SmithJones23");
    CHECK((r.found && r.id == u"SmithJones24" && r.alias));
    CHECK_FALSE(resolved("cite", "Nobody99").found);
}

TEST_CASE("The IDs a text mentions, and the ID being typed")
{
    const QString first = QStringLiteral("See \\ref{4KX7} and \\cite[p.~3]{SmithJones24, Other,}.");
    const QString text = first + QStringLiteral("\n% \\ref{HIDDEN}\n\\paperref{ 7RT2 } and \\reference{NOT}\n");
    const std::vector<IdMention> mentions = idMentions(text);
    REQUIRE(mentions.size() == 4);
    CHECK(mentions[0].command == QStringLiteral("ref"));
    CHECK(mentions[0].name == QStringLiteral("4KX7"));
    CHECK(mentions[0].line == 1);
    CHECK(mentions[0].start == 9);
    CHECK(mentions[0].length == 4);
    CHECK(mentions[1].command == QStringLiteral("cite"));
    CHECK(mentions[1].start == first.indexOf(QStringLiteral("SmithJones24")));
    CHECK(mentions[1].length == 12);
    CHECK(mentions[2].name == QStringLiteral("Other"));
    CHECK(mentions[2].start == first.indexOf(QStringLiteral("Other")));
    CHECK(mentions[3].command == QStringLiteral("paperref"));
    CHECK(mentions[3].name == QStringLiteral("7RT2"));
    CHECK(mentions[3].line == 3);
    CHECK(mentions[3].start == 11);

    std::optional<IdTyping> typing = idBeingTyped(u"Text \\ref{");
    REQUIRE(typing);
    CHECK(typing->command == QStringLiteral("ref"));
    CHECK(typing->typed.isEmpty());
    CHECK(typing->start == 10);
    typing = idBeingTyped(u"\\cite{A, Sm");
    REQUIRE(typing);
    CHECK(typing->command == QStringLiteral("cite"));
    CHECK(typing->typed == QStringLiteral("Sm"));
    CHECK(typing->start == 9);
    typing = idBeingTyped(u"\\paperref[x]{7R");
    REQUIRE(typing);
    CHECK(typing->command == QStringLiteral("paperref"));
    CHECK(typing->typed == QStringLiteral("7R"));
    CHECK_FALSE(idBeingTyped(u"\\ref{4K} more"));
    CHECK_FALSE(idBeingTyped(u"% \\ref{"));
    CHECK_FALSE(idBeingTyped(u"\\reference{"));
    CHECK_FALSE(idBeingTyped(u"\\label{"));
}
