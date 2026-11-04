# `loom new-tag`, `new-statement`, `edit-statement`, `new-proof` and `edit-proof` from the command
# line (Specs/m1-core.md, "The `loom` command"): exit codes 0–3, the output and the JSON shapes,
# and finding the project from a folder inside it. LoomCoreTests covers the details.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -P StatementsCli.cmake

# Runs loom in `dir` and fails the test unless it exits with `expected`.
#   expected: the exit code wanted.  out_var: set to loom's stdout.  dir: the working folder.
#   ARGN: loom's arguments.
function(run_loom_in dir expected out_var)
    execute_process(COMMAND "${LOOM}" ${ARGN} WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "loom ${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

# Runs loom in the project.
function(run_loom expected out_var)
    run_loom_in("${PROJECT}" "${expected}" out ${ARGN})
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

# Fails unless `text` matches the regular expression `pattern`.
function(expect_match what text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: expected a match for ${pattern}, got: ${text}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/gitconfig" "[user]\n\tname = Loom Test\n\temail = test@example.com\n")
set(ENV{GIT_CONFIG_GLOBAL} "${WORK}/gitconfig")
set(ENV{GIT_CONFIG_NOSYSTEM} 1)

set(PROJECT "${WORK}/Demo")
run_loom_in("${WORK}" 0 out new-project "${PROJECT}" --name Demo)

# 0: a tag, a statement, a proof, and edits; found from a folder inside the project.
run_loom_in("${PROJECT}/TeX" 0 out new-tag energy-estimates --kind technique --title "Energy estimates")
expect_match("new-tag" "${out}" "^energy-estimates\n$")

run_loom(0 out new-statement --kind lemma --title "Energy" --body "Text." --tag energy-estimates)
expect_match("new-statement" "${out}" "^[0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z]\n$")
string(STRIP "${out}" lemma)
string(TOLOWER "${lemma}" lemma_typed)

file(WRITE "${WORK}/body.tex" "First line.\r\nSecond line.\r\n")
run_loom(0 out new-statement --kind theorem --title "Decay" --body-file "${WORK}/body.tex" --json)
expect_match("new-statement --json" "${out}" "^{\"id\": \"([0-9A-Z]+)\", \"path\": \"Statements/[0-9A-Z]+\\.tex\"}\n$")
string(REGEX REPLACE "^{\"id\": \"([0-9A-Z]+)\".*$" "\\1" theorem "${out}")
file(READ "${PROJECT}/Statements/${theorem}.tex" text)
expect_match("the body from a file" "${text}" "\n\\+\\+\\+\nFirst line\\.\nSecond line\\.\n$")

run_loom(0 out new-proof "${theorem}" --level complete --body "By the lemma." --premise "${lemma_typed}")
expect_match("new-proof" "${out}" "^${theorem}\\.a\n$")
run_loom(0 out new-proof "${theorem}" --level sketch --body "Roughly." --json)
expect_match("new-proof --json" "${out}" "^{\"id\": \"${theorem}\\.b\", \"path\": \"Proofs/${theorem}/${theorem}\\.b\\.tex\"}\n$")

run_loom(0 out edit-proof "${theorem}.a" --level sketch)
expect_match("edit-proof" "${out}" "^${theorem}\\.a: changed level\\.\n$")
run_loom(0 out edit-proof "${theorem}.a" --level sketch --json)
expect_match("edit-proof, no change" "${out}" "^{\"id\": \"${theorem}\\.a\", \"changed\": \\[\\], \"outdated_checks\": \\[\\]}\n$")
run_loom_in("${WORK}" 0 out edit-statement "${theorem}" --title "Decay, sharp" --verified --json --project "${PROJECT}")
expect_match("edit-statement --json" "${out}" "^{\"id\": \"${theorem}\", \"changed\": \\[\"title\", \"verified\"\\], \"outdated_checks\": \\[\\]}\n$")

# 1: refused by a rule, with the JSON error on stdout; nothing written.
run_loom(1 out new-statement --kind lemma --title "X" --body "X" --tag no-such-tag)
run_loom(1 out new-proof ZZZZ --level complete --body "X" --json)
expect_match("new-proof of nothing" "${out}" "^{\"error\": {\"code\": \"ref-unknown\", \"message\": \"`ZZZZ` isn't a statement in this project\\.\"}}\n$")
run_loom(1 out edit-statement "${theorem}" --remove-stated-using "${lemma}")
run_loom(1 out new-tag energy-estimates --kind technique --title "Again")

# 2: wrong usage.
run_loom(2 out new-statement --kind lemma --body "X")
run_loom(2 out new-statement --kind lemma --title "X" --body "X" --body-file "${WORK}/body.tex")
run_loom(2 out new-statement --kind lemma --title "X")
run_loom(2 out new-statement --kind lemma --title "X" --body "X" --colour blue)
run_loom(2 out edit-statement "${theorem}")
run_loom(2 out edit-statement)
run_loom(2 out new-proof --level complete --body "X")
run_loom(2 out new-tag --kind other --title "X")
run_loom(2 out edit-proof "${theorem}.a" --level)
run_loom(2 out new-statement --kind lemma --title "X" --body "X" --project)

# 3: can't work here.
file(MAKE_DIRECTORY "${WORK}/Empty")
run_loom(3 out new-statement --kind lemma --title "X" --body "X" --project "${WORK}/Empty" --json)
expect_match("no project" "${out}" "^{\"error\": {\"code\": \"not-a-project\", ")
run_loom(3 out new-statement --kind lemma --title "X" --body-file "${WORK}/missing.tex")

message(STATUS "statements CLI: passed")
