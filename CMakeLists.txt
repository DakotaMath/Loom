# Loom's build. Every choice here comes from Specs/m1-core.md, "Toolchain".
cmake_minimum_required(VERSION 3.28)

project(Loom VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Programs land in one folder (build/<preset>/bin, plus /Debug or /Release on Windows),
# so they are easy to find and run.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

option(LOOM_WARNINGS_AS_ERRORS "Treat warnings in Loom's own code as errors (CI turns this on)" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/Source/CMake")
include(LoomCompilerOptions)
include(LoomCoreLinkCheck)
include(LoomDependencies)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Test)
qt_standard_project_setup()

# On Windows nothing puts Qt's DLLs next to our programs, so the tests get Qt's bin folder
# on their PATH instead (see Tests/CMakeLists.txt). Fail here, not in a test, if it's wrong.
set(LOOM_QT_BIN_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
if(WIN32 AND NOT EXISTS "${LOOM_QT_BIN_DIR}/Qt6Core.dll")
    message(FATAL_ERROR "Can't find Qt6Core.dll in ${LOOM_QT_BIN_DIR}. Is CMAKE_PREFIX_PATH Qt's msvc2022_64 folder?")
endif()

enable_testing()

add_subdirectory(Source/LoomCore)
add_subdirectory(Source/LoomCli)
add_subdirectory(Source/LoomApp)
add_subdirectory(Tests)

# Last, once every target that could link into LoomCore exists.
loom_check_no_ui_links(LoomCore)
