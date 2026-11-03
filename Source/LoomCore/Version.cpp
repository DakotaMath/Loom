#include "LoomCore/Version.h"

namespace loom {

std::string_view versionString()
{
    // LOOM_VERSION comes from CMake, so the version is written in one place only.
    return LOOM_VERSION;
}

} // namespace loom
