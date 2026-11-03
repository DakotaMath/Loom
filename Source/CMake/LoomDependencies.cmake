# toml++ and Catch2, fetched at configure time and pinned by URL and SHA-256
# (Specs/m1-core.md, "Toolchain"; why each one: Wiki/decisions/002 and 003).
# SYSTEM keeps their headers' warnings out of our /W4 and -Wall builds.
include(FetchContent)

# A commit, not the v3.4.0 tag: that tag is from 2023 and parser fixes have landed since.
FetchContent_Declare(tomlplusplus
    URL https://github.com/marzer/tomlplusplus/archive/3c71044e1e1bcfc50391d0d93e63609295ad3e18.tar.gz
    URL_HASH SHA256=70b5da4a3adc5ddba987ea04404b03710915e32cbdaff8f979e6c14e25547ced
    SYSTEM)

FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz
    URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
    SYSTEM)

FetchContent_MakeAvailable(tomlplusplus Catch2)

# For include(Catch) and catch_discover_tests().
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
