# LoomCore: the project model, formats, validation and status, shared by `loom` and LoomApp.
# No UI code: it links Qt6::Core and toml++ only, and the top-level CMakeLists.txt fails the
# configure if anything adds a UI module (Source/CMake/LoomCoreLinkCheck.cmake).
add_library(LoomCore STATIC
    Version.cpp
    Version.h)

target_include_directories(LoomCore PUBLIC "${PROJECT_SOURCE_DIR}/Source")
target_compile_definitions(LoomCore PRIVATE LOOM_VERSION="${PROJECT_VERSION}")
target_link_libraries(LoomCore
    PUBLIC Qt6::Core
    PRIVATE tomlplusplus::tomlplusplus)
loom_set_compiler_options(LoomCore)
