# LoomAppTests: Qt Test tests for the UI. CI on Linux runs them with QT_QPA_PLATFORM=offscreen.
qt_add_executable(LoomAppTests
    MainWindowTests.cpp)
target_link_libraries(LoomAppTests PRIVATE LoomAppLib LoomTestSupport Qt6::Test)
target_compile_definitions(LoomAppTests PRIVATE
    # The demo project, for the test that opens a copy of it outside git.
    LOOM_DEMO_PROJECT="${PROJECT_SOURCE_DIR}/Tests/Projects/Demo"
    # Pictures of the main screens (Specs/m2-statements-review.md, "Testing the UI": "UI tests save
    # the main screens to build/<preset>/screenshots/<test>.png").
    LOOM_SCREENSHOTS="${PROJECT_BINARY_DIR}/screenshots")
loom_set_compiler_options(LoomAppTests)

add_test(NAME LoomAppTests COMMAND LoomAppTests)
# Opening a project runs git, and creating one runs the commit guard, which needs `loom`.
add_dependencies(LoomAppTests loom)
if(WIN32)
    # Qt finds its platform plugin next to the Qt6Core.dll it loaded, so this is enough.
    set_tests_properties(LoomAppTests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()

# StatementsTabTests: the Statements tab (#35), its own program so each Qt Test class has its main().
qt_add_executable(StatementsTabTests
    StatementsTabTests.cpp)
target_link_libraries(StatementsTabTests PRIVATE LoomAppLib LoomTestSupport Qt6::Test)
target_compile_definitions(StatementsTabTests PRIVATE
    LOOM_SCREENSHOTS="${PROJECT_BINARY_DIR}/screenshots")
loom_set_compiler_options(StatementsTabTests)
add_test(NAME StatementsTabTests COMMAND StatementsTabTests)
add_dependencies(StatementsTabTests loom)
if(WIN32)
    set_tests_properties(StatementsTabTests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()

# CompileTests: compiling, the PDF pane and the Problems panel (#36). Its builds need TeX, hence
# the label; the cases that build skip without latexmk (Specs/m2-statements-review.md,
# "Dependencies and toolchain").
qt_add_executable(CompileTests
    CompileTests.cpp)
target_link_libraries(CompileTests PRIVATE LoomAppLib LoomTestSupport Qt6::Test)
target_compile_definitions(CompileTests PRIVATE
    LOOM_SCREENSHOTS="${PROJECT_BINARY_DIR}/screenshots")
loom_set_compiler_options(CompileTests)
add_test(NAME CompileTests COMMAND CompileTests)
add_dependencies(CompileTests loom)
set_tests_properties(CompileTests PROPERTIES LABELS tex)
if(WIN32)
    set_tests_properties(CompileTests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()
