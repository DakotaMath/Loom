#pragma once

#include <string_view>

namespace loom {

/// Loom's version, as `loom --version` prints it (Specs/m1-core.md, "The `loom` command").
/// @returns The version set in the top-level CMakeLists.txt's project(), as MAJOR.MINOR.PATCH
///          (for example "0.1.0"). No side effects.
std::string_view versionString();

} // namespace loom
