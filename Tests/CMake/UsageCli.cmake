# `loom` with no subcommand, an unknown one, and `loom help` (Specs/m1-core.md, "The `loom`
# command"): exit 2 with the problem named, the JSON error on stdout with `--json`, and help's
# usage text on stdout with exit 0.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -P UsageCli.cmake

# Runs loom and fails the test unless it exits with `expected`. Exit 2 is checked exactly, so a
# crash doesn't pass for a usage error.
#   expected: the exit code wanted.  out_var: set to loom's stdout; `<out_var>_err` to its stderr.
#   ARGN: loom's arguments.
function(run_loom expected out_var)
    execute_process(COMMAND "${LOOM}" ${ARGN} WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "loom ${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
    set(${out_var}_err "${err}" PARENT_SCOPE)
endfunction()

# Fails unless `text` matches the regular expression `pattern`.
function(expect_match what text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: expected a match for ${pattern}, got: ${text}")
    endif()
endfunction()

# Fails if `text` matches the regular expression `pattern`.
function(expect_no_match what text pattern)
    if(text MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: expected no match for ${pattern}, got: ${text}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# 2: no subcommand. The problem first, then the usage, on stderr.
run_loom(2 out)
expect_match("no subcommand" "${out_err}" "^`loom` needs a subcommand\\.\nUsage:\n")
expect_match("no subcommand, stdout" "${out}" "^$")
run_loom(2 out --json)
expect_match("no subcommand --json" "${out}"
    "^{\"error\": {\"code\": \"usage\", \"message\": \"`loom` needs a subcommand\\.\"}}\n$")

# 2: an unknown subcommand is named; with --json the error object holds only the problem.
run_loom(2 out frobnicate)
expect_match("unknown subcommand" "${out_err}" "^`frobnicate` isn't a loom subcommand\\. `loom help` lists them\\.\nUsage:\n")
run_loom(2 out frobnicate --json)
expect_match("unknown subcommand --json" "${out}"
    "^{\"error\": {\"code\": \"usage\", \"message\": \"`frobnicate` isn't a loom subcommand\\. `loom help` lists them\\.\"}}\n$")

# 0: help prints the usage on stdout; with a subcommand, only that subcommand's.
run_loom(0 out help)
expect_match("help" "${out}" "^Usage:\n  loom help \\[<subcommand>\\]\n")
expect_match("help, every subcommand" "${out}" "\n  loom validate \\[--staged\\]\n")
expect_match("help, stderr" "${out_err}" "^$")
run_loom(0 out help add-check)
expect_match("help add-check" "${out}"
    "^Usage:\n  loom add-check <proof-id> --by dakota\\|agent:<model> --verdict pass\\|needs-changes\\|fail\n[^\n]+\n[^\n]+\n[^\n]*--project <folder> and --json\\.\n$")
run_loom(0 out help status --json)
expect_match("help status --json" "${out}" "^{\"usage\": \"Usage:\\\\n  loom status \\[<id>\\]\\\\n")
expect_no_match("help status --json, only status" "${out}" "loom graph")

# 2: help for something that isn't a subcommand, or for two.
run_loom(2 out help frobnicate --json)
expect_match("help frobnicate" "${out}" "^{\"error\": {\"code\": \"usage\", \"message\": \"`frobnicate` isn't a loom subcommand\\. ")
run_loom(2 out help status graph)

# 2: the common options, wrong; a `--json` after the problem still gets the error as JSON.
run_loom(2 out status --project a --project b --json)
expect_match("--project twice" "${out}"
    "^{\"error\": {\"code\": \"usage\", \"message\": \"`--project` can be given only once\\.\"}}\n$")
run_loom(2 out status --json --project)
expect_match("--project without a folder" "${out}"
    "^{\"error\": {\"code\": \"usage\", \"message\": \"`--project` needs a folder\\.\"}}\n$")

message(STATUS "usage CLI: passed")
