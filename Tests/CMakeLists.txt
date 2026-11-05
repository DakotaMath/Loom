# Loom's tests, all run by `ctest`: Catch2 for LoomCore and `loom`, Qt Test for the UI,
# and checks of the build itself.
add_subdirectory(Support)
add_subdirectory(LoomCoreTests)
add_subdirectory(LoomAppTests)

# `loom --version` (Specs/m1-core.md, "The `loom` command"). Matching the output ignores the
# exit code, so the usage test runs through a script that checks the code exactly.
add_test(NAME loom.version COMMAND loom --version)
set_tests_properties(loom.version PROPERTIES PASS_REGULAR_EXPRESSION "^loom 0\\.1\\.0[\r\n]*$")
add_test(NAME loom.version-json COMMAND loom --version --json)
set_tests_properties(loom.version-json PROPERTIES PASS_REGULAR_EXPRESSION "^\\{\"version\": \"0\\.1\\.0\"\\}[\r\n]*$")
# No subcommand, an unknown one, and `loom help` (Tests/CMake/UsageCli.cmake).
add_test(NAME loom.usage
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/UsageCli"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/UsageCli.cmake")
set_tests_properties(loom.usage PROPERTIES PASS_REGULAR_EXPRESSION "usage CLI: passed")
# `loom new-project`: exit codes, output and the first commit (Tests/CMake/NewProjectCli.cmake).
add_test(NAME loom.new-project
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/NewProjectCli"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/NewProjectCli.cmake")
set_tests_properties(loom.new-project PROPERTIES PASS_REGULAR_EXPRESSION "new-project CLI: passed")
# The statement and proof subcommands (Tests/CMake/StatementsCli.cmake).
add_test(NAME loom.statements
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/StatementsCli"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/StatementsCli.cmake")
set_tests_properties(loom.statements PROPERTIES PASS_REGULAR_EXPRESSION "statements CLI: passed")
# The check subcommands (Tests/CMake/ChecksCli.cmake).
add_test(NAME loom.checks
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/ChecksCli"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/ChecksCli.cmake")
set_tests_properties(loom.checks PROPERTIES PASS_REGULAR_EXPRESSION "checks CLI: passed")
# The paper subcommands (Tests/CMake/PapersCli.cmake).
add_test(NAME loom.papers
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/PapersCli"
        "-DDATA=${CMAKE_CURRENT_SOURCE_DIR}/CMake/PapersCli"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/PapersCli.cmake")
set_tests_properties(loom.papers PROPERTIES PASS_REGULAR_EXPRESSION "papers CLI: passed")
# `status`, `graph` and `refute`, on a copy of the worked example (Tests/CMake/StatusCli.cmake).
add_test(NAME loom.status
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/StatusCli"
        "-DDATA=${CMAKE_CURRENT_SOURCE_DIR}/Projects/WorkedExample"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/StatusCli.cmake")
set_tests_properties(loom.status PROPERTIES PASS_REGULAR_EXPRESSION "status CLI: passed")
# `validate` and `install-hooks`, and the commit guard (Tests/CMake/ValidateCli.cmake).
add_test(NAME loom.validate
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/ValidateCli"
        "-DDATA=${CMAKE_CURRENT_SOURCE_DIR}/Projects/WorkedExample"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/ValidateCli.cmake")
set_tests_properties(loom.validate PROPERTIES PASS_REGULAR_EXPRESSION "validate CLI: passed")
# `loom build` on copies of the demo project (Tests/CMake/BuildCli.cmake). Its builds need TeX,
# hence the label.
add_test(NAME loom.build
    COMMAND "${CMAKE_COMMAND}"
        "-DLOOM=$<TARGET_FILE:loom>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/BuildCli"
        "-DDATA=${CMAKE_CURRENT_SOURCE_DIR}/Projects/Demo"
        "-DEDITS=${CMAKE_CURRENT_SOURCE_DIR}/Projects/Demo-edits"
        "-DKIT=${PROJECT_SOURCE_DIR}/Templates/project"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CMake/BuildCli.cmake")
set_tests_properties(loom.build PROPERTIES PASS_REGULAR_EXPRESSION "build CLI: passed" LABELS tex)
if(WIN32)
    set_tests_properties(loom.version loom.version-json loom.usage loom.new-project loom.statements loom.checks loom.papers loom.status loom.validate
        loom.build
        PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOOM_QT_BIN_DIR}")
endif()

# The LoomCore link check (Source/CMake/LoomCoreLinkCheck.cmake): a small project with stand-in
# Qt targets is configured once per case. Error text can be wrapped, so the patterns allow any
# whitespace between words.
set(link_check_dir "${CMAKE_CURRENT_SOURCE_DIR}/CMake/CoreLinkCheck")
set(ws "[ \t\r\n]+")
foreach(case clean direct-gui direct-widgets webengine through-target link-only versionless)
    add_test(NAME cmake.core-link-check.${case}
        COMMAND "${CMAKE_COMMAND}"
            -S "${link_check_dir}"
            -B "${CMAKE_CURRENT_BINARY_DIR}/CoreLinkCheck/${case}"
            -G "${CMAKE_GENERATOR}"
            "-DLOOM_CMAKE_DIR=${PROJECT_SOURCE_DIR}/Source/CMake"
            "-DLINK_CHECK_CASE=${case}")
endforeach()
set_tests_properties(cmake.core-link-check.clean PROPERTIES
    PASS_REGULAR_EXPRESSION "core-link-check: passed")
set_tests_properties(cmake.core-link-check.direct-gui PROPERTIES
    PASS_REGULAR_EXPRESSION "LoomCore${ws}must${ws}not${ws}link${ws}Qt6::Gui\\.")
set_tests_properties(cmake.core-link-check.direct-widgets PROPERTIES
    PASS_REGULAR_EXPRESSION "LoomCore${ws}must${ws}not${ws}link${ws}Qt6::Widgets\\.")
set_tests_properties(cmake.core-link-check.webengine PROPERTIES
    PASS_REGULAR_EXPRESSION "LoomCore${ws}must${ws}not${ws}link${ws}Qt6::WebEngineWidgets\\.")
set_tests_properties(cmake.core-link-check.through-target PROPERTIES
    PASS_REGULAR_EXPRESSION "LoomCore${ws}must${ws}not${ws}link${ws}Qt6::Gui${ws}\\(through${ws}Helper\\)")
set_tests_properties(cmake.core-link-check.link-only PROPERTIES
    PASS_REGULAR_EXPRESSION "LoomCore${ws}must${ws}not${ws}link${ws}Qt6::Widgets${ws}\\(through${ws}Helper\\)")
set_tests_properties(cmake.core-link-check.versionless PROPERTIES
    PASS_REGULAR_EXPRESSION "LoomCore${ws}must${ws}not${ws}link${ws}Qt::Widgets\\.")
