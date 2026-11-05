# LoomTestSupport: helpers shared by the Catch2 and Qt Test tests, without either framework.
# DemoProject puts Tests/Projects/Demo in a temporary git repository (Specs/m2-statements-review.md,
# "The demo project").
add_library(LoomTestSupport STATIC
    DemoProject.cpp
    DemoProject.h)
target_include_directories(LoomTestSupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(LoomTestSupport PUBLIC LoomCore)
target_compile_definitions(LoomTestSupport PRIVATE
    LOOM_DEMO_PROJECT="${PROJECT_SOURCE_DIR}/Tests/Projects/Demo"
    LOOM_DEMO_EDITS="${PROJECT_SOURCE_DIR}/Tests/Projects/Demo-edits"
    LOOM_PROJECT_KIT="${PROJECT_SOURCE_DIR}/Templates/project"
    # The commit guard runs `loom validate --staged`, so the demo's commit needs this build's `loom`.
    LOOM_PROGRAM="$<TARGET_FILE:loom>")
add_dependencies(LoomTestSupport loom)
loom_set_compiler_options(LoomTestSupport)
