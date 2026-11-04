// Finding circles in a graph of IDs.

#include "LoomCore/Cycles.h"

#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("No circles", "[cycles]")
{
    CHECK(loom::findCycles({}).empty());
    CHECK(loom::findCycles({{"A", {"B"}}, {"B", {"C"}}, {"C", {}}}).empty());
}

TEST_CASE("An edge to itself isn't a circle here", "[cycles]")
{
    CHECK(loom::findCycles({{"A", {"A"}}}).empty());
}

TEST_CASE("Each circle starts at its smallest ID and ends where it began", "[cycles]")
{
    CHECK(loom::findCycles({{"J3RW", {"H8KD"}}, {"H8KD", {"J3RW"}}}) == std::vector<QStringList>{{"H8KD", "J3RW", "H8KD"}});
    CHECK(loom::findCycles({{"C", {"A"}}, {"A", {"B"}}, {"B", {"C"}}}) == std::vector<QStringList>{{"A", "B", "C", "A"}});
}

TEST_CASE("Separate circles are reported once each, in order", "[cycles]")
{
    const std::vector<QStringList> cycles = loom::findCycles(
        {{"M", {"K"}}, {"K", {"M"}}, {"B", {"A"}}, {"A", {"B"}}, {"X", {"A"}}});
    CHECK(cycles == std::vector<QStringList>{{"A", "B", "A"}, {"K", "M", "K"}});
}

TEST_CASE("One group with two circles reports one circle, through its smallest ID", "[cycles]")
{
    // A → B → A and B → C → B share B, so A, B and C are one group: one circle, from A.
    CHECK(loom::findCycles({{"A", {"B"}}, {"B", {"A", "C"}}, {"C", {"B"}}}) == std::vector<QStringList>{{"A", "B", "A"}});
}

TEST_CASE("A successor that isn't a key is a node with no edges", "[cycles]")
{
    CHECK(loom::findCycles({{"A", {"Z"}}, {"B", {"Z", "A"}}}).empty());
    CHECK(loom::findCycles({{"A", {"Z", "B"}}, {"B", {"A"}}}) == std::vector<QStringList>{{"A", "B", "A"}});
}

TEST_CASE("The result doesn't depend on the order of the successors", "[cycles]")
{
    const std::vector<QStringList> expected{{"A", "B", "C", "A"}, {"K", "M", "K"}};
    CHECK(loom::findCycles({{"A", {"B"}}, {"B", {"C"}}, {"C", {"A", "Q"}}, {"K", {"M"}}, {"M", {"K"}}, {"Q", {}}}) == expected);
    CHECK(loom::findCycles({{"M", {"K"}}, {"K", {"M"}}, {"C", {"Q", "A"}}, {"B", {"C"}}, {"A", {"B"}}}) == expected);
}

TEST_CASE("Several circles, a self-loop among them, come out sorted by their first ID", "[cycles]")
{
    // The self-loop on X is the caller's to report (`premise-self`), so it isn't a circle here.
    const std::vector<QStringList> cycles = loom::findCycles(
        {{"X", {"X"}}, {"Q", {"R"}}, {"R", {"S"}}, {"S", {"Q"}}, {"D", {"E"}}, {"E", {"D"}}, {"N", {"P"}}, {"P", {"N"}}});
    CHECK(cycles == std::vector<QStringList>{{"D", "E", "D"}, {"N", "P", "N"}, {"Q", "R", "S", "Q"}});
}

TEST_CASE("The shortest way back is taken", "[cycles]")
{
    // A → B → C → D → A, with a shortcut C → A.
    CHECK(loom::findCycles({{"A", {"B"}}, {"B", {"C"}}, {"C", {"D", "A"}}, {"D", {"A"}}})
          == std::vector<QStringList>{{"A", "B", "C", "A"}});
}
