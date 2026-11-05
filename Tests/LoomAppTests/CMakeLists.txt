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

# IdTests: IDs in the editor (#37): the picker, titles beside IDs, hover cards, Ctrl+click. Its
# preview case builds with TeX and skips without latexmk; the rest need no TeX.
qt_add_executable(IdTests
    IdTests.cpp)
target_link_libraries(IdTests PRIVATE LoomAppLib LoomTestSupport Qt6::Test)
target_compile_definitions(IdTests PRIVATE
    LOOM_SCREENSHOTS="${PROJECT_BINARY_DIR}/screenshots")
loom_set_compiler_options(IdTests)
add_test(NAME IdTests COMMAND IdTests)
add_dependencies(IdTests loom)
if(WIN32)
    set_tests_properties(IdTests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()

# ClicksTests: status, checks and Dakota's clicks in the Statements tab (#38). No TeX.
qt_add_executable(ClicksTests
    ClicksTests.cpp)
target_link_libraries(ClicksTests PRIVATE LoomAppLib LoomTestSupport Qt6::Test)
target_compile_definitions(ClicksTests PRIVATE
    LOOM_SCREENSHOTS="${PROJECT_BINARY_DIR}/screenshots")
loom_set_compiler_options(ClicksTests)
add_test(NAME ClicksTests COMMAND ClicksTests)
add_dependencies(ClicksTests loom)
if(WIN32)
    set_tests_properties(ClicksTests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()
