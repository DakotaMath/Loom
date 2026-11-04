# `loom status`, `graph` and `refute` from the command line (Specs/m1-core.md, "The `loom`
# command"), on a copy of the worked example: exit codes 0–3, the output and the JSON shapes.
# LoomCoreTests covers every status and explanation.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -DDATA=<the WorkedExample folder> -P StatusCli.cmake

# Runs loom in the project and fails the test unless it exits with `expected`. Input comes from a
# file holding `yes`, so it is never a terminal, however the test is run: Dakota's `refute` must
# refuse even a piped yes.
#   expected: the exit code wanted.  out_var: set to loom's stdout.  ARGN: loom's arguments.
function(run_loom expected out_var)
    execute_process(COMMAND "${LOOM}" ${ARGN} WORKING_DIRECTORY "${PROJECT}" INPUT_FILE "${WORK}/yes.txt"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "loom ${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
    set(last_stderr "${err}" PARENT_SCOPE)
endfunction()

# Fails unless `text` matches the regular expression `pattern`.
function(expect_match what text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${what}: expected a match for ${pattern}, got: ${text}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/yes.txt" "yes\n")
file(COPY "${DATA}/" DESTINATION "${WORK}/WorkedExample")
set(PROJECT "${WORK}/WorkedExample")

# 0: the list and the counts; one statement's sentence; the JSON shapes.
run_loom(0 out status)
expect_match("status" "${out}" "^2M9Q  -            Weak solution of the model system\n3WQ6  extracted    ")
expect_match("status, a stub's gist as its title" "${out}" "\n7RT2  cited        Weak solutions with L\\^2 data")
expect_match("status, the counts" "${out}"
    "\n28 statements: 8 proved, 6 conditional, 4 unchecked, 2 sketch, 1 heuristic, 2 open, 1 cited, 1 extracted, 1 false, 2 without a status\\.\n$")
run_loom(0 out status 6bwt)
expect_match("status <id>" "${out}"
    "^6BWT conditional: 6BWT\\.a is complete and checked, but rests on 3WQ6, which is extracted \\(not yet checked against the paper's PDF\\)\\.\n$")

run_loom(0 out status --json)
expect_match("status --json, counts" "${out}"
    "\"counts\": {\"proved\": 8, \"conditional\": 6, \"unchecked\": 4, \"sketch\": 2, \"heuristic\": 1, \"open\": 2, \"cited\": 1, \"extracted\": 1, \"false\": 1, \"none\": 2}}\n$")
expect_match("status --json, a statement without status" "${out}"
    "^{\"statements\": \\[{\"id\": \"2M9Q\", \"kind\": \"definition\", \"title\": \"Weak solution of the model system\", \"paper\": null, \"status\": null, \"deciding_proof\": null, \"rests_on\": \\[\\], \"superseded_by\": null, \"explanation\": ")
expect_match("status --json, a stub" "${out}" "{\"id\": \"3WQ6\", \"kind\": \"lemma\", \"title\": \"[^\"]*\", \"paper\": \"SmithJones24\", \"status\": \"extracted\"")
run_loom(0 out status E8TB --json)
expect_match("status <id> --json" "${out}"
    "^{\"id\": \"E8TB\", \"kind\": \"theorem\", \"title\": \"Decay for large data\", \"paper\": null, \"status\": \"conditional\", \"deciding_proof\": \"E8TB\\.b\", \"rests_on\": \\[\"8CJA\"\\], \"superseded_by\": null, \"explanation\": \"E8TB conditional: [^\"]*\"}\n$")
run_loom(0 out status N7ZE --json)
expect_match("status <id> --json, superseded" "${out}" "\"status\": \"proved\", \"deciding_proof\": \"N7ZE\\.a\", \"rests_on\": \\[\\], \"superseded_by\": \"4KX7\"")

run_loom(0 out graph --json)
expect_match("graph --json, its shape" "${out}" "^{\"format\": 1, \"nodes\": \\[{\"id\": \"2M9Q\", \"kind\": \"definition\", \"title\": \"Weak solution of the model system\", \"status\": null, \"paper\": null, \"superseded_by\": null, \"tags\": \\[\\]}, ")
expect_match("graph --json, a proof" "${out}"
    "{\"id\": \"4KX7\\.a\", \"proves\": \"4KX7\", \"premises\": \\[\"5HN8\", \"9D3F\"\\], \"level\": \"complete\", \"fingerprint\": \"[0-9a-f]+\", \"checked\": true, \"supports\": true, \"gives\": \"complete-checked\", \"counting_agent_check\": 0, \"counting_dakota_check\": 1}")
expect_match("graph --json, a proof without checks" "${out}"
    "{\"id\": \"CQ2R\\.a\", \"proves\": \"CQ2R\", \"premises\": \\[\\], \"level\": \"heuristic\", \"fingerprint\": \"[0-9a-f]+\", \"checked\": false, \"supports\": true, \"gives\": \"heuristic\", \"counting_agent_check\": null, \"counting_dakota_check\": null}")
expect_match("graph --json, a failed proof" "${out}" "\"id\": \"D9XF\\.b\", [^}]*\"supports\": false, \"gives\": \"nothing\", \"counting_agent_check\": 1,")
expect_match("graph --json, edges and cycles" "${out}"
    "\\], \"stated_using\": \\[{\"from\": \"4KX7\", \"to\": \"2M9Q\"}\\], \"paper_premises\": \\[\\], \"cycles\": \\[\\[\"H8KD\", \"J3RW\"\\], \\[\"K6YM\", \"M1PQ\"\\]\\]}\n$")
# A hand-edited level holding `%1` is printed as it is, and the JSON stays valid.
set(cq2r "${PROJECT}/Proofs/CQ2R/CQ2R.a.tex")
file(READ "${cq2r}" cq2r_text)
string(REPLACE "level = \"heuristic\"" "level = \"%1\"" odd_level "${cq2r_text}")
file(WRITE "${cq2r}" "${odd_level}")
run_loom(0 out graph --json)
expect_match("graph --json, a level holding %1" "${out}"
    "{\"id\": \"CQ2R\\.a\", \"proves\": \"CQ2R\", \"premises\": \\[\\], \"level\": \"%1\", \"fingerprint\": \"[0-9a-f]+\", \"checked\": false, \"supports\": true, \"gives\": \"nothing\", \"counting_agent_check\": null, \"counting_dakota_check\": null}")
file(WRITE "${cq2r}" "${cq2r_text}")

# 1: refused. `refute` is Dakota's, and the test's input isn't a terminal; nothing is written.
file(READ "${PROJECT}/Statements/T9GB.tex" before)
run_loom(1 out refute T9GB --by G5TZ.a --json)
expect_match("refute without a terminal" "${out}"
    "^{\"error\": {\"code\": \"dakota-only\", \"message\": \"Only Dakota can refute a statement, and only at a terminal\\.\"}}\n$")
file(READ "${PROJECT}/Statements/T9GB.tex" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "refute without a terminal changed T9GB")
endif()
# What refute can check without Dakota is checked before he is asked, so these fail as
# themselves, not as `dakota-only`.
run_loom(1 out refute ZZZZ --by G5TZ.a --json)
expect_match("refute of nothing" "${out}" "^{\"error\": {\"code\": \"ref-unknown\", \"message\": \"`ZZZZ` isn't a statement in this project\\.\"}}\n$")
run_loom(1 out refute T9GB --by G5TZ.z --json)
expect_match("refute by nothing" "${out}" "^{\"error\": {\"code\": \"ref-unknown\", \"message\": \"`G5TZ\\.z` isn't a proof in this project\\.\"}}\n$")
run_loom(1 out refute G5TZ --by G5TZ.a --json)
expect_match("refute by its own proof" "${out}" "^{\"error\": {\"code\": \"refuted-self\", ")
run_loom(1 out refute F2NS --by G5TZ.a --json)
expect_match("refute again" "${out}" "^{\"error\": {\"code\": \"already-there\", ")
run_loom(1 out refute 7RT2 --by G5TZ.a --json)
expect_match("refute a stub" "${out}"
    "^{\"error\": {\"code\": \"ref-unknown\", \"message\": \"7RT2 is a result from SmithJones24, cited from the paper; only statements of this project can be refuted\\.\"}}\n$")
run_loom(1 out status ZZZZ --json)
expect_match("status of nothing" "${out}" "^{\"error\": {\"code\": \"ref-unknown\", \"message\": \"`ZZZZ` isn't a statement in this project\\.\"}}\n$")

# 2: wrong usage.
run_loom(2 out graph)
run_loom(2 out graph extra --json)
run_loom(2 out status 4KX7 5HN8)
run_loom(2 out status --colour blue)
run_loom(2 out refute T9GB)
run_loom(2 out refute --by G5TZ.a)

# 3: can't work here.
file(MAKE_DIRECTORY "${WORK}/Empty")
run_loom(3 out status --project "${WORK}/Empty" --json)
expect_match("no project" "${out}" "^{\"error\": {\"code\": \"not-a-project\", ")
run_loom(3 out graph --json --project "${WORK}/Empty")

message(STATUS "status CLI: passed")
