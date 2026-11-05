# `loom new-paper`, `new-stub`, `rekey` and `build-bib` from the command line (Specs/m1-core.md,
# "The `loom` command"): exit codes 0–3, the output and the JSON shapes. LoomCoreTests covers
# the details.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -DDATA=<PapersCli folder> -P PapersCli.cmake

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
    set(last_stderr "${err}" PARENT_SCOPE)
endfunction()

# Runs loom in the project.
function(run_loom expected out_var)
    run_loom_in("${PROJECT}" "${expected}" out ${ARGN})
    set(${out_var} "${out}" PARENT_SCOPE)
    set(last_stderr "${last_stderr}" PARENT_SCOPE)
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
run_loom(0 out new-tag model-system --kind equation --title "The model system")

# The second draft: the same authors, posted a year later, so the two share a key.
file(READ "${DATA}/draft.toml" draft)
string(REPLACE "2023-01-15" "2024-01-10" second "${draft}")
string(REPLACE "Energy decay for a model parabolic system" "A second paper" second "${second}")
file(WRITE "${WORK}/second.toml" "${second}")

# 0: new-paper computes the key; a second paper sharing it letters both.
run_loom(0 out new-paper "${DATA}/draft.toml")
expect_match("new-paper" "${out}" "^SmithJones24\n$")
run_loom(0 out new-paper "${WORK}/second.toml" --json)
expect_match("new-paper --json" "${out}"
    "^{\"key\": \"SmithJones24b\", \"path\": \"Papers/SmithJones24b/paper.toml\", \"renamed\": \\[{\"from\": \"SmithJones24\", \"to\": \"SmithJones24a\"}\\], \"unclicked\": \\[\\]}\n$")
expect_match("the rename is told" "${last_stderr}" "Renamed SmithJones24 to SmithJones24a")
if(NOT EXISTS "${PROJECT}/Papers/SmithJones24a/paper.toml" OR EXISTS "${PROJECT}/Papers/SmithJones24")
    message(FATAL_ERROR "new-paper didn't rename SmithJones24 to SmithJones24a")
endif()

# 0: new-stub, by an old key too.
run_loom(0 out new-stub SmithJones24 --kind theorem --gist "Weak solutions lose energy" --version arxiv-v1
    --at "Theorem 1.2, p. 4" --by agent:claude-opus-5-5)
expect_match("new-stub" "${out}" "^[0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z][0-9A-HJKMNP-TV-Z]\n$")
string(STRIP "${out}" stub)
file(READ "${PROJECT}/Papers/SmithJones24a/Statements/${stub}.toml" text)
expect_match("the stub" "${text}" "\n\\[locators\\]\narxiv-v1 = { at = \"Theorem 1\\.2, p\\. 4\", checked_against_pdf = false }\n$")
# A stub of another paper can't be a paper premise.
run_loom(1 out new-stub SmithJones24b --kind lemma --gist "G" --version published --at "Lemma 2.5"
    --by dakota --paper-premise "${stub}" --json)
expect_match("new-stub refused" "${out}" "^{\"error\": {\"code\": \"stub-paper-premise\"")
run_loom(0 out new-stub SmithJones24a --kind lemma --gist "Energy is monotone" --version published --at "Lemma 2.4" --by dakota --json)
expect_match("new-stub --json" "${out}" "^{\"id\": \"[0-9A-Z]+\", \"path\": \"Papers/SmithJones24a/Statements/[0-9A-Z]+\\.toml\"}\n$")

# 0: rekey of a key that is right; build-bib.
run_loom(0 out rekey SmithJones24a)
expect_match("rekey" "${out}" "^SmithJones24a: already right\\.\n$")
run_loom(0 out rekey SmithJones24a --json)
expect_match("rekey --json" "${out}" "^{\"from\": \"SmithJones24a\", \"to\": \"SmithJones24a\", \"renamed\": \\[\\]}\n$")
run_loom(0 out build-bib)
expect_match("build-bib" "${out}" "refs\\.bib\n$")
expect_match("build-bib warns" "${last_stderr}" "refs\\.bib uses unverified fields: SmithJones24a authors, title")
file(READ "${PROJECT}/.loom/build/refs.bib" bib)
expect_match("refs.bib" "${bib}" "^@article{SmithJones24a,\n.*\n}\n\n@article{SmithJones24b,\n")
run_loom_in("${PROJECT}/TeX" 0 out build-bib --out refs.bib --json)
expect_match("build-bib --json" "${out}" "^{\"path\": \"TeX/refs\\.bib\", \"problems\": \\[{\"severity\": \"warning\", \"code\": \"bib-unverified\"")
if(NOT EXISTS "${PROJECT}/TeX/refs.bib")
    message(FATAL_ERROR "build-bib --out didn't write TeX/refs.bib")
endif()

# 1: refused by a rule.
file(READ "${DATA}/draft.toml" keyed)
file(WRITE "${WORK}/keyed.toml" "key = \"Smith24\"\n${keyed}")
run_loom(1 out new-paper "${WORK}/keyed.toml" --json)
expect_match("new-paper wrong key" "${out}" "^{\"error\": {\"code\": \"paper-key-rule\"")
run_loom(1 out new-stub Nobody99 --kind lemma --gist "G" --version published --at "L" --by dakota)
run_loom(1 out rekey Nobody99)
# A third paper sharing the key would be SmithJones24c, and a folder of that name is already there
# (A3): refused, and the folder is left alone.
string(REPLACE "2023-01-15" "2024-06-01" third "${draft}")
string(REPLACE "Energy decay for a model parabolic system" "A third paper" third "${third}")
file(WRITE "${WORK}/third.toml" "${third}")
file(WRITE "${PROJECT}/Papers/SmithJones24c/notes.md" "Mine.\n")
run_loom(1 out new-paper "${WORK}/third.toml" --json)
expect_match("new-paper onto a folder" "${out}"
    "^{\"error\": {\"code\": \"paper-exists\", \"message\": \"`Papers/SmithJones24c/` already exists\\. ")
file(GLOB left RELATIVE "${PROJECT}/Papers/SmithJones24c" "${PROJECT}/Papers/SmithJones24c/*")
if(NOT left STREQUAL "notes.md")
    message(FATAL_ERROR "new-paper wrote into Papers/SmithJones24c: ${left}")
endif()
file(REMOVE_RECURSE "${PROJECT}/Papers/SmithJones24c")
# 0: a draft that verifies fields itself: Loom sets them to false, since verifying is Dakota's
# click, and says which (S16).
string(REGEX REPLACE "(\njournal = [^\n]*verified = )false" "\\12026-11-03" verifying "${third}")
string(REGEX REPLACE "(\nvolume = [^\n]*verified = )false" "\\12026-11-03" verifying "${verifying}")
# The entry's own `verified` can't be false, so it is left out.
file(WRITE "${WORK}/verifying.toml" "verified = 2026-11-03\n${verifying}")
run_loom(0 out new-paper "${WORK}/verifying.toml" --json)
expect_match("new-paper --json, dates set to false" "${out}"
    "^{\"key\": \"SmithJones24c\", \"path\": \"Papers/SmithJones24c/paper.toml\", \"renamed\": \\[\\], \"unclicked\": \\[\"verified\", \"published\\.journal\", \"published\\.volume\"\\]}\n$")
expect_match("new-paper says so" "${last_stderr}"
    "^Left out `verified`, since verifying is Dakota's click\\.\nSet to false, since verifying is Dakota's click: published\\.journal and published\\.volume\\.\n$")
file(READ "${PROJECT}/Papers/SmithJones24c/paper.toml" text)
if(text MATCHES "2026-11-03")
    message(FATAL_ERROR "new-paper kept a verified date: ${text}")
endif()
# build-bib never writes over a file of the project, even through a link or a short name.
file(READ "${PROJECT}/Papers/SmithJones24c/paper.toml" before)
if(WIN32)
    file(TO_NATIVE_PATH "${PROJECT}/TeX/papers" link_native)
    file(TO_NATIVE_PATH "${PROJECT}/Papers" target_native)
    execute_process(COMMAND cmd /c mklink /J "${link_native}" "${target_native}" RESULT_VARIABLE linked OUTPUT_QUIET)
else()
    file(CREATE_LINK "${PROJECT}/Papers" "${PROJECT}/TeX/papers" RESULT linked SYMBOLIC)
endif()
if(NOT linked STREQUAL "0")
    message(FATAL_ERROR "couldn't link TeX/papers to Papers: ${linked}")
endif()
run_loom(1 out build-bib --out TeX/papers/SmithJones24c/paper.toml --json)
expect_match("build-bib --out through a link" "${out}" "^{\"error\": {\"code\": \"bib-out-project-file\", ")
run_loom(1 out build-bib --out TeX/papers/refs.bib --json)
expect_match("build-bib --out into Papers/ through a link" "${out}" "^{\"error\": {\"code\": \"bib-out-project-file\", ")
if(EXISTS "${PROJECT}/Papers/refs.bib")
    message(FATAL_ERROR "build-bib wrote Papers/refs.bib through a link")
endif()
# Only the link goes, never what it points to.
if(WIN32)
    execute_process(COMMAND cmd /c rmdir "${link_native}")
else()
    file(REMOVE "${PROJECT}/TeX/papers")
endif()
if(WIN32)
    # The 8.3 short name of `Statements`, if the volume makes them: `STATEM~1`.
    file(MAKE_DIRECTORY "${PROJECT}/Statements")
    file(WRITE "${WORK}/short.bat" "@for %%I in (\"%~1\") do @echo %%~sI\n")
    file(TO_NATIVE_PATH "${WORK}/short.bat" short_bat)
    file(TO_NATIVE_PATH "${PROJECT}/Statements" statements_native)
    execute_process(COMMAND cmd /c "${short_bat}" "${statements_native}" OUTPUT_VARIABLE short OUTPUT_STRIP_TRAILING_WHITESPACE)
    get_filename_component(short_name "${short}" NAME)
    if(short_name STREQUAL "" OR short_name STREQUAL "Statements")
        message(STATUS "no 8.3 short names on this volume; skipping the short-name case")
    else()
        run_loom(1 out build-bib --out "${PROJECT}/${short_name}/refs.bib" --json)
        expect_match("build-bib --out through a short name" "${out}" "^{\"error\": {\"code\": \"bib-out-project-file\", ")
        if(EXISTS "${PROJECT}/Statements/refs.bib")
            message(FATAL_ERROR "build-bib wrote Statements/refs.bib through the short name ${short_name}")
        endif()
    endif()
endif()
file(READ "${PROJECT}/Papers/SmithJones24c/paper.toml" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "build-bib changed a paper.toml through a link")
endif()
file(READ "${PROJECT}/loom.toml" before)
run_loom(1 out build-bib --out loom.toml --json)
expect_match("build-bib --out loom.toml" "${out}" "^{\"error\": {\"code\": \"bib-out-project-file\", ")
file(READ "${PROJECT}/loom.toml" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "build-bib --out loom.toml changed loom.toml")
endif()

# 2: wrong usage.
run_loom(2 out new-paper)
run_loom(2 out new-stub SmithJones24a --kind lemma --version published --at "L" --by dakota)
run_loom(2 out rekey)
run_loom(2 out build-bib extra)

# 3: can't work here.
run_loom(3 out new-paper "${WORK}/missing.toml")
run_loom(3 out build-bib --out "${WORK}/outside.bib")
run_loom_in("${WORK}" 3 out rekey SmithJones24a)

message(STATUS "papers CLI: passed")
