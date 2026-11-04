# LoomCoreTests: Catch2 tests for LoomCore. catch_discover_tests registers each TEST_CASE with
# CTest by running the program to list them, which on Windows needs Qt's DLLs on the PATH.
add_executable(LoomCoreTests
    BibliographyTests.cpp
    ChecksTests.cpp
    CyclesTests.cpp
    EditsTests.cpp
    FingerprintTests.cpp
    FrontMatterTests.cpp
    IdsTests.cpp
    NewProjectTests.cpp
    PaperEditsTests.cpp
    PaperFixture.h
    PaperKeysTests.cpp
    PapersTests.cpp
    ProjectContentsTests.cpp
    ProjectFilesTests.cpp
    ProjectFixture.h
    ProjectTomlTests.cpp
    ProofChecksTests.cpp
    StatementsAndProofsTests.cpp
    TestSupport.cpp
    TestSupport.h)
target_link_libraries(LoomCoreTests PRIVATE LoomCore Catch2::Catch2WithMain)
loom_set_compiler_options(LoomCoreTests)

# A stand-in for git, for the new-project failures a real git can't show on demand.
add_executable(loom-fake-git FakeGit.cpp)
target_link_libraries(loom-fake-git PRIVATE Qt6::Core)
loom_set_compiler_options(loom-fake-git)
add_dependencies(LoomCoreTests loom-fake-git)
target_compile_definitions(LoomCoreTests PRIVATE LOOM_FAKE_GIT="$<TARGET_FILE:loom-fake-git>")

include(Catch)
set(dl_paths "")
if(WIN32)
    set(dl_paths DL_PATHS "${LOOM_QT_BIN_DIR}")
endif()
catch_discover_tests(LoomCoreTests ${dl_paths})
