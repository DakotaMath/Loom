# LoomCore: the project model, formats, validation and status, shared by `loom` and LoomApp.
# No UI code: it links Qt6::Core and toml++ only, and the top-level CMakeLists.txt fails the
# configure if anything adds a UI module (Source/CMake/LoomCoreLinkCheck.cmake).
add_library(LoomCore STATIC
    Git.cpp
    Git.h
    NewProject.cpp
    NewProject.h
    Problem.cpp
    Problem.h
    ProjectFiles.cpp
    ProjectFiles.h
    ProjectToml.cpp
    ProjectToml.h
    TomlWriter.cpp
    TomlWriter.h
    Version.cpp
    Version.h)

target_include_directories(LoomCore PUBLIC "${PROJECT_SOURCE_DIR}/Source")
target_compile_definitions(LoomCore PRIVATE LOOM_VERSION="${PROJECT_VERSION}")
target_link_libraries(LoomCore
    PUBLIC Qt6::Core
    PRIVATE tomlplusplus::tomlplusplus)
loom_set_compiler_options(LoomCore)

# The project kit, compiled in so `loom` is one self-contained program (Specs/m1-core.md,
# "The project folder"). It's read from `:/kit/`; Templates/project/ holds `.gitignore` and
# `.gitattributes` without their dots, so they don't act on Loom's own repo.
set(kit_dir "${PROJECT_SOURCE_DIR}/Templates/project")
qt_add_resources(LoomCore project_kit
    PREFIX "/kit"
    BASE "${kit_dir}"
    FILES
        "${kit_dir}/CLAUDE.md"
        "${kit_dir}/TeX/macros.tex"
        "${kit_dir}/TeX/preamble.tex"
        "${kit_dir}/gitattributes"
        "${kit_dir}/gitignore")
