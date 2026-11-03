# LoomCoreTests: Catch2 tests for LoomCore. catch_discover_tests registers each TEST_CASE with
# CTest by running the program to list them, which on Windows needs Qt's DLLs on the PATH.
add_executable(LoomCoreTests
    VersionTests.cpp)
target_link_libraries(LoomCoreTests PRIVATE LoomCore Catch2::Catch2WithMain)
loom_set_compiler_options(LoomCoreTests)

include(Catch)
set(dl_paths "")
if(WIN32)
    set(dl_paths DL_PATHS "${LOOM_QT_BIN_DIR}")
endif()
catch_discover_tests(LoomCoreTests ${dl_paths})
