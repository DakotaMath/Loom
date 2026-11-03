# LoomAppTests: Qt Test tests for the UI. CI on Linux runs them with QT_QPA_PLATFORM=offscreen.
qt_add_executable(LoomAppTests
    MainWindowTests.cpp)
target_link_libraries(LoomAppTests PRIVATE LoomAppLib Qt6::Test)
loom_set_compiler_options(LoomAppTests)

add_test(NAME LoomAppTests COMMAND LoomAppTests)
if(WIN32)
    # Qt finds its platform plugin next to the Qt6Core.dll it loaded, so this is enough.
    set_tests_properties(LoomAppTests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()
