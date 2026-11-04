# `loom fingerprint`, `add-check`, `merge-checks` and `--outdate-checks` from the command line
# (Specs/m1-core.md, "The `loom` command" and "Checks"): exit codes 0–3, the output and the JSON
# shapes, and Dakota's subcommands refusing without a terminal. LoomCoreTests covers the details.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -P ChecksCli.cmake

# Runs loom in `dir` and fails the test unless it exits with `expected`. Input comes from a file
# holding `yes`, so it is never a terminal: Dakota's subcommands must refuse even a piped yes.
#   expected: the exit code wanted.  out_var: set to loom's stdout; `<out_var>_err` to its stderr.
#   dir: the working folder.  ARGN: loom's arguments.
function(run_loom_in dir expected out_var)
    execute_process(COMMAND "${LOOM}" ${ARGN} WORKING_DIRECTORY "${dir}" INPUT_FILE "${WORK}/yes.txt"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "loom ${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
    set(${out_var}_err "${err}" PARENT_SCOPE)
endfunction()

# Runs loom in the project.
function(run_loom expected out_var)
    run_loom_in("${PROJECT}" "${expected}" out ${ARGN})
    set(${out_var} "${out}" PARENT_SCOPE)
    set(${out_var}_err "${out_err}" PARENT_SCOPE)
endfunction()

# Fails unless `text` matches the regular expression `pattern`.
function(expect_match what text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: expected a match for ${pattern}, got: ${text}")
    endif()
endfunction()

set(tag "[0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z]")
set(fp "[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]")
set(date "[0-9-]+T[0-9:]+(Z|[+-][0-9][0-9]:[0-9][0-9])")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/yes.txt" "yes\n")
file(WRITE "${WORK}/gitconfig" "[user]\n\tname = Loom Test\n\temail = test@example.com\n")
set(ENV{GIT_CONFIG_GLOBAL} "${WORK}/gitconfig")
set(ENV{GIT_CONFIG_NOSYSTEM} 1)

set(PROJECT "${WORK}/Demo")
run_loom_in("${WORK}" 0 out new-project "${PROJECT}" --name Demo)
run_loom(0 out new-statement --kind lemma --title "Energy" --body "An energy estimate.")
string(STRIP "${out}" lemma)
run_loom(0 out new-statement --kind theorem --title "Decay" --body "Decay.")
string(STRIP "${out}" theorem)
run_loom(0 out new-proof "${theorem}" --level complete --body "By the lemma." --premise "${lemma}")
run_loom(0 out new-proof "${theorem}" --level complete --body "Directly.")

# 0: fingerprints and an agent's checks.
run_loom(0 out fingerprint "${theorem}.a")
expect_match("fingerprint" "${out}" "^${fp}\n$")
string(STRIP "${out}" fp_a)
run_loom(0 out fingerprint "${theorem}.b" --json)
expect_match("fingerprint --json" "${out}" "^{\"proof\": \"${theorem}\\.b\", \"fingerprint\": \"(${fp})\"}\n$")
string(REGEX REPLACE "^.*\"fingerprint\": \"(${fp})\".*$" "\\1" fp_b "${out}")

run_loom(0 out add-check "${theorem}.a" --by agent:claude-opus-5-5 --verdict pass --fingerprint "${fp_a}"
    --record Records/2026-11/job-0001.md)
expect_match("add-check" "${out}"
    "^${theorem}\\.a: added check 1, agent:claude-opus-5-5's pass at ${date} \\(fingerprint ${fp_a}\\)\\.\n${theorem}\\.a isn't checked\\.\n$")
run_loom(0 out add-check "${theorem}.a" --by agent:claude-opus-5-5 --verdict needs-changes --fingerprint "${fp_a}"
    --record Records/2026-11/job-0002.md --json)
expect_match("add-check --json" "${out}"
    "^{\"proof\": \"${theorem}\\.a\", \"path\": \"Proofs/${theorem}/${theorem}\\.a\\.checks\\.toml\", \"index\": 1, \"check\": {\"by\": \"agent:claude-opus-5-5\", \"date\": \"${date}\", \"fingerprint\": \"${fp_a}\", \"verdict\": \"needs-changes\", \"changes\": null, \"record\": \"Records/2026-11/job-0002\\.md\", \"note\": null}, \"checked\": false, \"supports\": true, \"counting_agent_check\": 1, \"counting_dakota_check\": null}\n$")
file(READ "${PROJECT}/Proofs/${theorem}/${theorem}.a.checks.toml" checks)
expect_match("the check history" "${checks}" "^\\[\\[checks\\]\\]\nby = \"agent:claude-opus-5-5\"\n.*\n\n\\[\\[checks\\]\\]\n.*verdict = \"needs-changes\"\n")

# 1: refused by a rule, with the JSON error on stdout.
run_loom(1 out add-check "${theorem}.a" --by agent:claude-opus-5-5 --verdict pass --record Records/x.md --json)
expect_match("no fingerprint" "${out}" "^{\"error\": {\"code\": \"fingerprint-needed\", ")
run_loom(1 out add-check "${theorem}.a" --by agent:claude-opus-5-5 --verdict pass --record Records/x.md --fingerprint 000000000000)
expect_match("a changed proof" "${out_err}"
    "^${theorem}\\.a changed after you read it \\(you read 000000000000, it is now ${fp_a}\\)\\. Read it again before checking it\\.\n$")
run_loom(1 out add-check "${theorem}.a" --by dakota --verdict pass --json)
expect_match("Dakota's check without a terminal" "${out}"
    "^{\"error\": {\"code\": \"dakota-only\", \"message\": \"Only Dakota can add Dakota's checks, and only at a terminal\\.\"}}\n$")
run_loom(1 out add-check "${theorem}.a" --by agent:claude-opus-5-5 --verdict accept --record Records/x.md --fingerprint "${fp_a}")
run_loom(1 out fingerprint ZZZZ.a)

# Dakota's pass, as his click at a terminal would write it, makes theorem.b checked; then editing
# the text it read is refused, and --outdate-checks is Dakota's.
run_loom(0 out add-check "${theorem}.b" --by agent:claude-opus-5-5 --verdict pass --fingerprint "${fp_b}" --record Records/x.md)
file(APPEND "${PROJECT}/Proofs/${theorem}/${theorem}.b.checks.toml"
    "\n[[checks]]\nby = \"dakota\"\ndate = 2099-01-01T00:00:00Z\nfingerprint = \"${fp_b}\"\nverdict = \"pass\"\n")
file(WRITE "${WORK}/new-body.tex" "A different proof.\n")
run_loom(1 out edit-proof "${theorem}.b" --body-file "${WORK}/new-body.tex" --json)
expect_match("edit-proof outdating checks" "${out}"
    "^{\"error\": {\"code\": \"outdates-checks\", \"message\": \"Loom didn't change ${theorem}\\.b, because it would outdate the checks of ${theorem}\\.b: ")
run_loom(1 out edit-proof "${theorem}.b" --body-file "${WORK}/new-body.tex" --outdate-checks --json)
expect_match("--outdate-checks without a terminal" "${out}"
    "^{\"error\": {\"code\": \"dakota-only\", \"message\": \"Only Dakota can outdate checks, and only at a terminal\\.\"}}\n$")
run_loom(1 out edit-statement "${theorem}" --body-file "${WORK}/new-body.tex")
run_loom(0 out edit-proof "${theorem}.b" --level sketch --outdate-checks --json)
expect_match("an edit outdating nothing" "${out}" "^{\"id\": \"${theorem}\\.b\", \"changed\": \\[\"level\"\\], \"outdated_checks\": \\[\\]}\n$")
run_loom(0 out edit-statement "${lemma}" --body-file "${WORK}/new-body.tex" --json)
expect_match("an edit of an unchecked proof's premise" "${out}" "^{\"id\": \"${lemma}\", \"changed\": \\[\"body\"\\], \"outdated_checks\": \\[\\]}\n$")

# merge-checks: 0 merged into ours, saying so if it put back a deleted check; 1 left for a person.
set(entry_1 "[[checks]]\nby = \"agent:x\"\ndate = 2026-11-04T10:00:00-06:00\nfingerprint = \"3c8e1f0a92d4\"\nverdict = \"pass\"\nrecord = \"Records/a.md\"\n")
set(entry_2 "[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T11:00:00-06:00\nfingerprint = \"3c8e1f0a92d4\"\nverdict = \"pass\"\n")
set(entry_3 "[[checks]]\nby = \"agent:y\"\ndate = 2026-11-04T12:00:00-06:00\nfingerprint = \"3c8e1f0a92d4\"\nverdict = \"fail\"\nrecord = \"Records/b.md\"\n")
file(WRITE "${WORK}/base.toml" "${entry_1}")
file(WRITE "${WORK}/ours.toml" "${entry_1}\n${entry_3}")
file(WRITE "${WORK}/theirs.toml" "${entry_1}\n${entry_2}")
run_loom_in("${WORK}" 0 out merge-checks base.toml ours.toml theirs.toml)
expect_match("merge-checks prints nothing" "${out}${out_err}" "^$")
file(READ "${WORK}/ours.toml" merged)
if(NOT merged STREQUAL "${entry_1}\n${entry_2}\n${entry_3}")
    message(FATAL_ERROR "merge-checks: unexpected result:\n${merged}")
endif()
file(WRITE "${WORK}/ours.toml" "${entry_2}")
run_loom_in("${WORK}" 0 out merge-checks base.toml ours.toml theirs.toml)
expect_match("merge-checks restoring" "${out_err}" "^A check was deleted on one side of the merge; Loom kept it, since checks are never removed\\.\n$")
file(WRITE "${WORK}/ours.toml" "[[checks]\n")
run_loom_in("${WORK}" 1 out merge-checks base.toml ours.toml theirs.toml)
file(READ "${WORK}/ours.toml" unmerged)
expect_match("a failed merge leaves ours alone" "${unmerged}" "^\\[\\[checks\\]\n$")

# 2: wrong usage.
run_loom(2 out add-check "${theorem}.a" --verdict pass)
run_loom(2 out add-check --by dakota --verdict pass)
run_loom(2 out add-check "${theorem}.a" --by dakota --verdict needs-changes --changes x --changes-file "${WORK}/new-body.tex")
run_loom(2 out fingerprint)
run_loom_in("${WORK}" 2 out merge-checks base.toml ours.toml)

# 3: can't work here.
run_loom_in("${WORK}" 3 out fingerprint "${theorem}.a")
run_loom_in("${WORK}" 3 out merge-checks base.toml missing.toml theirs.toml)

message(STATUS "checks CLI: passed")
