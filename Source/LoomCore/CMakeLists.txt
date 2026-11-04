# LoomCore: the project model, formats, validation and status, shared by `loom` and LoomApp.
# No UI code: it links Qt6::Core and toml++ only, and the top-level CMakeLists.txt fails the
# configure if anything adds a UI module (Source/CMake/LoomCoreLinkCheck.cmake).
add_library(LoomCore STATIC
    Bibliography.cpp
    Bibliography.h
    Checks.cpp
    Checks.h
    Cycles.cpp
    Cycles.h
    Edits.cpp
    EditSession.cpp
    EditSession.h
    Edits.h
    FieldReader.cpp
    FieldReader.h
    Fingerprint.cpp
    Fingerprint.h
    FrontMatter.cpp
    FrontMatter.h
    Git.cpp
    Git.h
    Hooks.cpp
    Hooks.h
    Ids.cpp
    Ids.h
    NewProject.cpp
    NewProject.h
    PaperEdits.cpp
    PaperEdits.h
    PaperKeys.cpp
    PaperKeys.h
    Papers.cpp
    Papers.h
    Problem.cpp
    Problem.h
    ProjectContents.cpp
    ProjectContents.h
    ProjectFiles.cpp
    ProjectFiles.h
    ProjectToml.cpp
    ProjectToml.h
    ProofChecks.cpp
    ProofChecks.h
    StatementsAndProofs.cpp
    StatementsAndProofs.h
    Status.cpp
    Status.h
    TomlFields.cpp
    TomlFields.h
    TomlWriter.cpp
    TomlWriter.h
    Validation.cpp
    Validation.h
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

# The pre-commit hook `loom install-hooks` writes (Templates/hooks/pre-commit), read from `:/hooks/`.
set(hooks_dir "${PROJECT_SOURCE_DIR}/Templates/hooks")
qt_add_resources(LoomCore loom_hooks
    PREFIX "/hooks"
    BASE "${hooks_dir}"
    FILES
        "${hooks_dir}/pre-commit")
