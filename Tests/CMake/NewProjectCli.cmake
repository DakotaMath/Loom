# `loom new-project` from the command line (Specs/m1-core.md, "The `loom` command"): exit codes
# 0–3, the printed path, the JSON shapes and one commit. LoomCoreTests covers the details.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -P NewProjectCli.cmake

# Runs loom and fails the test unless it exits with `expected`.
#   expected: the exit code wanted.  out_var: set to loom's stdout.  ARGN: loom's arguments.
function(run_loom expected out_var)
    execute_process(COMMAND "${LOOM}" ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "loom ${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# A git identity of the test's own, and none of the machine's settings.
file(WRITE "${WORK}/gitconfig" "[user]\n\tname = Loom Test\n\temail = test@example.com\n")
set(ENV{GIT_CONFIG_GLOBAL} "${WORK}/gitconfig")
set(ENV{GIT_CONFIG_NOSYSTEM} 1)

# 2: wrong usage.
run_loom(2 out new-project)
run_loom(2 out new-project "${WORK}/Demo")
run_loom(2 out new-project "${WORK}/Demo" --name Demo --colour blue)
run_loom(2 out new-project "${WORK}/Demo" "${WORK}/Other" --name Demo)
run_loom(2 out new-project "${WORK}/Demo" --name Demo --name Other --json)
if(NOT out STREQUAL "{\"error\": {\"code\": \"usage\", \"message\": \"`--name` can be given only once.\"}}\n")
    message(FATAL_ERROR "Unexpected JSON for a repeated --name: ${out}")
endif()
# The folder to create is new-project's argument; `--project` names an existing project.
run_loom(2 out new-project "${WORK}/Demo" --name Demo --project "${WORK}/Elsewhere" --json)
if(NOT out MATCHES "^{\"error\": {\"code\": \"usage\", \"message\": \"`loom new-project` doesn't take `--project`")
    message(FATAL_ERROR "Unexpected JSON for new-project with --project: ${out}")
endif()
if(EXISTS "${WORK}/Demo" OR EXISTS "${WORK}/Elsewhere")
    message(FATAL_ERROR "new-project created a folder after wrong usage")
endif()

# 1: refused by a rule, with the JSON error on stdout.
file(WRITE "${WORK}/Busy/notes.txt" "mine")
run_loom(1 out new-project "${WORK}/Busy" --name Demo --json)
if(NOT out MATCHES "^{\"error\": {\"code\": \"folder-not-empty\", \"message\": \"`[^\"]*Busy` isn't empty\\. Loom creates projects only in a new or empty folder\\.\"}}\n$")
    message(FATAL_ERROR "Unexpected JSON for a non-empty folder: ${out}")
endif()

# 3: can't work here.
run_loom(3 out new-project "${WORK}/Missing/Demo" --name Demo)

# 0: created; the path is printed, natively, and JSON-escaped with --json.
run_loom(0 out new-project "${WORK}/Demo" --name Demo --idea "An idea.")
file(TO_NATIVE_PATH "${WORK}/Demo" native)
if(NOT out STREQUAL "${native}\n")
    message(FATAL_ERROR "Expected the path `${native}`, got: ${out}")
endif()
run_loom(0 out new-project "${WORK}/Json" --name Json --json)
file(TO_NATIVE_PATH "${WORK}/Json" native)
string(REPLACE "\\" "\\\\" escaped "${native}")
if(NOT out STREQUAL "{\"path\": \"${escaped}\"}\n")
    message(FATAL_ERROR "Expected {\"path\": \"${escaped}\"}, got: ${out}")
endif()

find_program(GIT git REQUIRED)
execute_process(COMMAND "${GIT}" -C "${WORK}/Demo" log --format=%s OUTPUT_VARIABLE log COMMAND_ERROR_IS_FATAL ANY)
if(NOT log STREQUAL "Create project Demo\n")
    message(FATAL_ERROR "Expected one commit, `Create project Demo`; git log says: ${log}")
endif()

message(STATUS "new-project CLI: passed")
