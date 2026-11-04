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

TEST_CASE("The shortest way back is taken", "[cycles]")
{
    // A → B → C → D → A, with a shortcut C → A.
    CHECK(loom::findCycles({{"A", {"B"}}, {"B", {"C"}}, {"C", {"D", "A"}}, {"D", {"A"}}})
          == std::vector<QStringList>{{"A", "B", "C", "A"}});
}
