# Compiler settings for Loom's own targets (Specs/m1-core.md, "Toolchain").
# Dependencies keep their own settings: these are applied per target, never globally.

# Applies Loom's warning and source-encoding flags to one of Loom's own targets.
#   target: the target to configure.
# Side effects: adds PRIVATE compile options; makes warnings errors when LOOM_WARNINGS_AS_ERRORS is ON.
function(loom_set_compiler_options target)
    if(MSVC)
        # /utf-8 is required: without it MSVC reads sources in the system code page and
        # mangles non-ASCII string literals.
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ${LOOM_WARNINGS_AS_ERRORS})
endfunction()
