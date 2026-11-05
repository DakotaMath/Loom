# LoomCoreTests: Catch2 tests for LoomCore. catch_discover_tests registers each TEST_CASE with
# CTest by running the program to list them, which on Windows needs Qt's DLLs on the PATH.
add_executable(LoomCoreTests
    AppLockTests.cpp
    BibliographyTests.cpp
    BuildDocumentTests.cpp
    BuildTests.cpp
    ChecksTests.cpp
    CyclesTests.cpp
    DraftsTests.cpp
    EditorSavesTests.cpp
    EditsTests.cpp
    FingerprintTests.cpp
    FrontMatterTests.cpp
    HooksTests.cpp
    IdSearchTests.cpp
    IdsTests.cpp
    NewProjectTests.cpp
    PaperEditsTests.cpp
    PaperFixture.h
    PaperKeysTests.cpp
    PapersTests.cpp
    ProjectContentsTests.cpp
    ProjectFilesTests.cpp
    ProjectFixture.h
    PreviewTests.cpp
    ProjectTomlTests.cpp
    ProofChecksTests.cpp
    SetupCheckTests.cpp
    StatementsAndProofsTests.cpp
    StatusTests.cpp
    SyncTexTests.cpp
    TestSupport.cpp
    TestSupport.h
    TexLogTests.cpp
    ValidationTests.cpp)
target_link_libraries(LoomCoreTests PRIVATE LoomCore LoomTestSupport Catch2::Catch2WithMain)
# The worked example of Specs/m1-core.md, a project folder the status tests read (and never write).
target_compile_definitions(LoomCoreTests PRIVATE LOOM_WORKED_EXAMPLE="${PROJECT_SOURCE_DIR}/Tests/Projects/WorkedExample")
loom_set_compiler_options(LoomCoreTests)

# A stand-in for git, for the new-project failures a real git can't show on demand.
add_executable(loom-fake-git FakeGit.cpp)
target_link_libraries(loom-fake-git PRIVATE Qt6::Core)
loom_set_compiler_options(loom-fake-git)
add_dependencies(LoomCoreTests loom-fake-git)
target_compile_definitions(LoomCoreTests PRIVATE LOOM_FAKE_GIT="$<TARGET_FILE:loom-fake-git>")

# A stand-in for each tool *Check my setup* runs, copied under each tool's name (FakeTool.cpp).
# Plain C++, so it runs with a PATH holding only stand-ins, without Qt's DLLs.
add_executable(loom-fake-tool FakeTool.cpp)
loom_set_compiler_options(loom-fake-tool)
# std::getenv is fine here; MSVC would warn that it's "unsafe".
target_compile_definitions(loom-fake-tool PRIVATE _CRT_SECURE_NO_WARNINGS)
add_dependencies(LoomCoreTests loom-fake-tool)
target_compile_definitions(LoomCoreTests PRIVATE LOOM_FAKE_TOOL="$<TARGET_FILE:loom-fake-tool>")

# The pre-commit hook runs `loom validate --staged`, so the tests that commit need this build's `loom`.
add_dependencies(LoomCoreTests loom)
target_compile_definitions(LoomCoreTests PRIVATE LOOM_PROGRAM="$<TARGET_FILE:loom>")
# The demo project of M2 (Tests/Projects/Demo), read in place by tests that only read it; the
# tests that build or commit copy it first (Tests/Support/DemoProject).
target_compile_definitions(LoomCoreTests PRIVATE LOOM_DEMO_PROJECT="${PROJECT_SOURCE_DIR}/Tests/Projects/Demo")
# Logs saved from real builds of the demo, for the log tests.
target_compile_definitions(LoomCoreTests PRIVATE LOOM_TEX_LOGS="${CMAKE_CURRENT_SOURCE_DIR}/TexLogs")
# The test project with one of every problem (Tests/Projects/EveryProblem), copied before each use.
target_compile_definitions(LoomCoreTests PRIVATE LOOM_EVERY_PROBLEM="${PROJECT_SOURCE_DIR}/Tests/Projects/EveryProblem")

include(Catch)
set(dl_paths "")
if(WIN32)
    set(dl_paths DL_PATHS "${LOOM_QT_BIN_DIR}")
endif()
# Tags become CTest labels: the builds that need TeX are tagged [tex] (Specs/m2-statements-review.md,
# "Dependencies and toolchain"), so `ctest -L tex` runs them and `ctest -LE tex` leaves them out.
catch_discover_tests(LoomCoreTests ${dl_paths} ADD_TAGS_AS_LABELS)
