# `loom validate` and `loom install-hooks` from the command line (Specs/m1-core.md, "Validation"
# and "The `loom` command"): exit codes 0–3, the text and JSON output, and the commit guard: a
# commit with an error is refused by the pre-commit hook, and goes through once fixed.
# LoomCoreTests covers what each check finds.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first>
#               -DDATA=<the worked example> -P ValidateCli.cmake

# Runs a program and fails the test unless it exits with `expected`.
#   expected: the exit code wanted.  out_var, err_var: set to stdout and stderr.  ARGN: the command.
# `loom` writes UTF-8; without `ENCODING UTF-8`, CMake on Windows decodes it in the console's code
# page and `→` arrives as `ΓåÆ` (Wiki/gotchas/cmake/execute-process-encoding.md).
function(run expected out_var err_var)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING UTF-8)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
    set(${err_var} "${err}" PARENT_SCOPE)
endfunction()

# Fails the test unless `actual` is the expected text: the arguments after it, joined. They're read
# one by one as ARGV<n>, since `${ARGN}` would split them as a list, where a `[` (as in JSON)
# stops `;` from separating elements.
function(expect_equal what actual)
    set(expected "")
    math(EXPR last "${ARGC} - 1")
    foreach(i RANGE 2 ${last})
        string(APPEND expected "${ARGV${i}}")
    endforeach()
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${what}:\nexpected: ${expected}\nactual:   ${actual}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# A git identity of the test's own, and none of the machine's settings.
file(WRITE "${WORK}/gitconfig" "[user]\n\tname = Loom Test\n\temail = test@example.com\n")
set(ENV{GIT_CONFIG_GLOBAL} "${WORK}/gitconfig")
set(ENV{GIT_CONFIG_NOSYSTEM} 1)
set(ENV{GIT_CEILING_DIRECTORIES} "${WORK}")
find_program(GIT git REQUIRED)

# 2: wrong usage.
run(2 out err "${LOOM}" validate --colour)
run(2 out err "${LOOM}" validate extra)
run(2 out err "${LOOM}" install-hooks extra)

# 3: no project here, or --staged without a repository.
file(MAKE_DIRECTORY "${WORK}/Empty")
run(3 out err "${LOOM}" validate --project "${WORK}/Empty")
run(3 out err "${LOOM}" validate --project "${WORK}/Empty" --json)
if(NOT out MATCHES "^{\"error\": {\"code\": \"not-a-project\", ")
    message(FATAL_ERROR "Unexpected JSON for a folder without a project: ${out}")
endif()
file(COPY "${DATA}/" DESTINATION "${WORK}/Worked")
run(3 out err "${LOOM}" validate --staged --project "${WORK}/Worked")

# 1: the worked example's problems, as text (problems on stderr, the count on stdout) and JSON.
run(1 out err "${LOOM}" validate --project "${WORK}/Worked")
expect_equal("the count line" "${out}" "2 errors, 3 warnings.\n")
expect_equal("the problems" "${err}"
    "Proofs/H8KD/H8KD.a.tex:4: error: Premises go round in a circle: H8KD → J3RW → H8KD. A statement can't help prove itself.\n"
    "Proofs/K6YM/K6YM.a.tex:4: error: Premises go round in a circle: K6YM → M1PQ → K6YM. A statement can't help prove itself.\n"
    "Proofs/P3QA/P3QA.a.tex:4: warning: P3QA.a uses N7ZE, which is superseded by 4KX7.\n"
    "Proofs/T9GB/T9GB.a.tex:4: warning: T9GB.a uses F2NS, which is false.\n"
    "Statements/F2NS.tex:5: warning: F2NS is marked false, but F2NS.a would prove it. One of them is wrong.\n")
run(1 out err "${LOOM}" validate --project "${WORK}/Worked" --json)
expect_equal("the JSON" "${out}"
    "{\"errors\": 2, \"warnings\": 3, \"problems\": ["
    "{\"severity\": \"error\", \"code\": \"premise-cycle\", \"file\": \"Proofs/H8KD/H8KD.a.tex\", \"line\": 4, \"message\": \"Premises go round in a circle: H8KD → J3RW → H8KD. A statement can't help prove itself.\"}, "
    "{\"severity\": \"error\", \"code\": \"premise-cycle\", \"file\": \"Proofs/K6YM/K6YM.a.tex\", \"line\": 4, \"message\": \"Premises go round in a circle: K6YM → M1PQ → K6YM. A statement can't help prove itself.\"}, "
    "{\"severity\": \"warning\", \"code\": \"premise-superseded\", \"file\": \"Proofs/P3QA/P3QA.a.tex\", \"line\": 4, \"message\": \"P3QA.a uses N7ZE, which is superseded by 4KX7.\"}, "
    "{\"severity\": \"warning\", \"code\": \"premise-false\", \"file\": \"Proofs/T9GB/T9GB.a.tex\", \"line\": 4, \"message\": \"T9GB.a uses F2NS, which is false.\"}, "
    "{\"severity\": \"warning\", \"code\": \"refuted-but-proved\", \"file\": \"Statements/F2NS.tex\", \"line\": 5, \"message\": \"F2NS is marked false, but F2NS.a would prove it. One of them is wrong.\"}"
    "]}\n")
# 3: a project from a newer Loom.
file(WRITE "${WORK}/Worked/loom.toml" "format = 2\nname = \"Future\"\n")
run(3 out err "${LOOM}" validate --project "${WORK}/Worked")
run(3 out err "${LOOM}" install-hooks --project "${WORK}/Worked")

# 0: a new project is clean, and new-project installed the hook and the merge driver.
run(0 out err "${LOOM}" new-project "${WORK}/Demo" --name Demo)
run(0 out err "${LOOM}" validate --project "${WORK}/Demo")
expect_equal("a clean project" "${out}" "No problems.\n")
run(0 out err "${LOOM}" validate --project "${WORK}/Demo" --json)
expect_equal("a clean project's JSON" "${out}" "{\"errors\": 0, \"warnings\": 0, \"problems\": []}\n")
if(NOT EXISTS "${WORK}/Demo/.git/hooks/pre-commit")
    message(FATAL_ERROR "new-project didn't install the pre-commit hook")
endif()
run(0 out err "${GIT}" -C "${WORK}/Demo" config --local --get merge.loom-checks.driver)
expect_equal("the merge driver" "${out}" "loom merge-checks %O %A %B\n")

# install-hooks again: Loom's own hook is replaced, not kept.
run(0 out err "${LOOM}" install-hooks --project "${WORK}/Demo" --json)
if(NOT out MATCHES "^{\"hook\": \"[^\"]*pre-commit\", \"moved_hook\": null, \"merge_driver\": \"loom-checks\"}\n$")
    message(FATAL_ERROR "Unexpected JSON from install-hooks: ${out}")
endif()

# 3: install-hooks works only in a Loom project at the top of its own repository, and writes only
# into that repository's git directory (A1). Each refusal writes nothing.
set(foreign_hook "#!/bin/sh\nexec make check\n")
file(MAKE_DIRECTORY "${WORK}/Plain")
run(0 out err "${GIT}" -C "${WORK}/Plain" init -q -b main)
file(WRITE "${WORK}/Plain/.git/hooks/pre-commit" "${foreign_hook}")
run(3 out err "${LOOM}" install-hooks --project "${WORK}/Plain" --json)
if(NOT out MATCHES "^{\"error\": {\"code\": \"not-a-project\", ")
    message(FATAL_ERROR "Unexpected JSON for install-hooks without loom.toml: ${out}")
endif()
file(WRITE "${WORK}/Plain/Inner/loom.toml" "format = 1\nname = \"Inner\"\ncreated = 2026-11-04\n")
run(3 out err "${LOOM}" install-hooks --project "${WORK}/Plain/Inner" --json)
if(NOT out MATCHES "^{\"error\": {\"code\": \"hooks-not-top-level\", ")
    message(FATAL_ERROR "Unexpected JSON for install-hooks below the top of a repository: ${out}")
endif()
file(READ "${WORK}/Plain/.git/hooks/pre-commit" hook)
expect_equal("the repository's own hook" "${hook}" "${foreign_hook}")
if(EXISTS "${WORK}/Plain/.git/hooks/pre-commit.before-loom")
    message(FATAL_ERROR "install-hooks moved the hook of a repository that isn't a Loom project")
endif()
file(WRITE "${WORK}/Shared/pre-commit" "${foreign_hook}")
run(0 out err "${GIT}" -C "${WORK}/Demo" config --local core.hooksPath "${WORK}/Shared")
run(3 out err "${LOOM}" install-hooks --project "${WORK}/Demo" --json)
if(NOT out MATCHES "^{\"error\": {\"code\": \"hooks-path-outside\", \"message\": \"Git runs hooks from `[^`]*Shared` \\(`core\\.hooksPath`\\), outside this project")
    message(FATAL_ERROR "Unexpected JSON for install-hooks with core.hooksPath set: ${out}")
endif()
file(GLOB shared RELATIVE "${WORK}/Shared" "${WORK}/Shared/*")
expect_equal("the shared hooks folder" "${shared}" "pre-commit")
file(READ "${WORK}/Shared/pre-commit" hook)
expect_equal("the shared hook" "${hook}" "${foreign_hook}")
run(0 out err "${GIT}" -C "${WORK}/Demo" config --local --unset core.hooksPath)

# The commit guard (Specs/m1-core.md, Verification, "Commit guard"): break a statement's front
# matter and the commit is refused with the message; fix it and the commit goes through.
run(0 out err "${LOOM}" new-statement --project "${WORK}/Demo" --kind lemma --title "A lemma" --body "Every x is fine.")
string(STRIP "${out}" tag)
set(statement "${WORK}/Demo/Statements/${tag}.tex")
file(READ "${statement}" good)
string(REPLACE "kind = " "knd = " broken "${good}")
file(WRITE "${statement}" "${broken}")
run(0 out err "${GIT}" -C "${WORK}/Demo" add --all)
run(1 out err "${GIT}" -C "${WORK}/Demo" commit -q -m "Broken")
if(NOT err MATCHES "Statements/${tag}\\.tex:3: error: `knd` isn't a field of a statement\\.")
    message(FATAL_ERROR "The refused commit didn't say why: ${err}")
endif()
file(WRITE "${statement}" "${good}")
run(0 out err "${GIT}" -C "${WORK}/Demo" add --all)
run(0 out err "${GIT}" -C "${WORK}/Demo" commit -q -m "Fixed")
run(0 out err "${GIT}" -C "${WORK}/Demo" log --format=%s)
expect_equal("the commits" "${out}" "Fixed\nCreate project Demo\n")

message(STATUS "validate CLI: passed")
