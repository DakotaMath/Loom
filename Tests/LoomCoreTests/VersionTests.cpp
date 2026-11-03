// The version string (Specs/m1-core.md, "The `loom` command": `loom --version` prints `loom 0.1.0`).

#include "LoomCore/Version.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The version is 0.1.0", "[version]")
{
    REQUIRE(loom::versionString() == "0.1.0");
}
