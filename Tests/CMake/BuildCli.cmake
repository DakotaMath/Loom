# `loom build` from the command line (Specs/m2-statements-review.md, "`loom build`"): exit codes
# 0–3, the text and JSON output, and `--rev`, on copies of the demo project. LoomCoreTests covers
# what builds contain. The builds that need TeX run when `latexmk` is found; without it the
# script checks `tex-missing` instead, unless LOOM_REQUIRE_TEX=1 asks for TeX, as CI's Linux job does.
# Run as: cmake -DLOOM=<path to loom> -DWORK=<scratch folder, emptied first> -DDATA=<the demo>
#               -DEDITS=<the demo's overlays> -DKIT=<the project kit> -P BuildCli.cmake

# Runs a program and fails the test unless it exits with `expected`.
#   expected: the exit code wanted.  out_var, err_var: set to stdout and stderr.  ARGN: the command.
# `loom` writes UTF-8; without `ENCODING UTF-8`, CMake on Windows decodes it in the console's code
# page (Wiki/gotchas/cmake/execute-process-encoding.md).
function(run expected out_var err_var)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING UTF-8)
    if(NOT code STREQUAL "${expected}")
        message(FATAL_ERROR "${ARGN}: expected exit ${expected}, got ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
    set(${err_var} "${err}" PARENT_SCOPE)
endfunction()

# Fails the test unless `actual` is the expected text: the arguments after it, joined.
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

# Fails the test unless `actual` matches the regular expression `pattern`.
function(expect_match what actual pattern)
    if(NOT actual MATCHES "${pattern}")
        message(FATAL_ERROR "${what}:\nexpected to match: ${pattern}\nactual: ${actual}")
    endif()
endfunction()

# Copies an overlay over the demo, file by file. Not `file(COPY)`: it skips a file whose
# destination has the same time to the second, and the demo and its overlays are written together
# (Wiki/gotchas/cmake/file-copy-skips-same-timestamp.md).
function(apply_overlay name)
    file(GLOB_RECURSE files RELATIVE "${EDITS}/${name}" "${EDITS}/${name}/*")
    foreach(file IN LISTS files)
        get_filename_component(folder "${demo}/${file}" DIRECTORY)
        file(MAKE_DIRECTORY "${folder}")
        file(COPY_FILE "${EDITS}/${name}/${file}" "${demo}/${file}")
    endforeach()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# A git identity of the test's own, and none of the machine's settings.
file(WRITE "${WORK}/gitconfig" "[user]\n\tname = Loom Test\n\temail = test@example.com\n")
set(ENV{GIT_CONFIG_GLOBAL} "${WORK}/gitconfig")
set(ENV{GIT_CONFIG_NOSYSTEM} 1)
set(ENV{GIT_CEILING_DIRECTORIES} "${WORK}")
find_program(GIT git REQUIRED)
find_program(LATEXMK latexmk)
if(NOT LATEXMK AND "$ENV{LOOM_REQUIRE_TEX}" STREQUAL "1")
    message(FATAL_ERROR "latexmk not found, and LOOM_REQUIRE_TEX=1")
endif()

# The demo, as a repository with one commit through the commit guard.
set(demo "${WORK}/Demo")
file(COPY "${DATA}/" DESTINATION "${demo}")
file(COPY_FILE "${KIT}/gitignore" "${demo}/.gitignore")
file(COPY_FILE "${KIT}/gitattributes" "${demo}/.gitattributes")
run(0 out err "${GIT}" -C "${demo}" init -b main)
run(0 out err "${LOOM}" install-hooks --project "${demo}")
run(0 out err "${GIT}" -C "${demo}" add -A)
run(0 out err "${GIT}" -C "${demo}" commit --quiet -m "The demo project")
run(0 head err "${GIT}" -C "${demo}" rev-parse HEAD)
string(STRIP "${head}" head)

# 2: wrong usage.
run(2 out err "${LOOM}" build --project "${demo}")
run(2 out err "${LOOM}" build C4JK --mode sideways --project "${demo}")
run(2 out err "${LOOM}" build C4JK --timeout 0 --project "${demo}")
run(2 out err "${LOOM}" build C4JK --colour --project "${demo}")
run(2 out err "${LOOM}" build C4JK D5MN --project "${demo}")

# 3: no project here; a commit git can't find.
file(MAKE_DIRECTORY "${WORK}/Empty")
run(3 out err "${LOOM}" build C4JK --project "${WORK}/Empty" --json)
expect_match("JSON for a folder without a project" "${out}" "^{\"error\": {\"code\": \"not-a-project\", ")
run(3 out err "${LOOM}" build C4JK --rev no-such-branch --project "${demo}" --json)
expect_match("JSON for a commit git can't find" "${out}" "^{\"error\": {\"code\": \"git-failed\", ")

# 1: refusals, which need no TeX.
run(1 out err "${LOOM}" build 7RT2 --project "${demo}")
expect_equal("a stub" "${err}" "7RT2 is a result from SmithJones24; it's built only as part of a statement's chain.\n")
run(1 out err "${LOOM}" build ZZZZ --project "${demo}" --json)
expect_equal("an unknown ID" "${out}"
    "{\"error\": {\"code\": \"ref-unknown\", \"message\": \"`ZZZZ` isn't a statement or proof in this project.\"}}\n")
apply_overlay(review)
string(SUBSTRING "${head}" 0 7 short)
run(1 out err "${LOOM}" build M7QR --rev HEAD --project "${demo}")
expect_equal("new since the commit" "${err}" "`M7QR` isn't a statement or proof at `HEAD` (${short}).\n")

if(NOT LATEXMK)
    # 3: no TeX.
    run(3 out err "${LOOM}" build C4JK --project "${demo}" --json)
    expect_match("JSON without TeX" "${out}" "^{\"error\": {\"code\": \"tex-missing\", ")
    message(STATUS "latexmk not found: the builds that need TeX were skipped")
    message("build CLI: passed")
    return()
endif()

# 0: built; the PDF's path on stdout, project-relative.
run(0 out err "${LOOM}" build C4JK --project "${demo}")
expect_equal("the PDF" "${out}" ".loom/build/C4JK/alone/main.pdf\n")
expect_equal("no problems" "${err}" "")
if(NOT EXISTS "${demo}/.loom/build/C4JK/alone/main.pdf")
    message(FATAL_ERROR "No PDF at .loom/build/C4JK/alone/main.pdf")
endif()

# JSON, with a warning; then the same build again, reused.
set(f7rs_problem "{\"severity\": \"warning\", \"code\": \"paperref-no-locator\", \"file\": \"Proofs/F7RS/F7RS.a.tex\", \"line\": 8, \"message\": \"SmithJones24's cited version `published` has no locator for 3WQ6, so the citation has no theorem number.\"}")
run(0 out err "${LOOM}" build f7rs --mode premises --verbose --project "${demo}" --json)
string(REPLACE "\"seconds\": " "\"seconds\": <" out "${out}")
string(REGEX REPLACE "<[0-9]+\\.[0-9]" "<n" out "${out}")
expect_equal("the JSON" "${out}"
    "{\"target\": \"F7RS\", \"mode\": \"premises\", \"verbose\": true, \"rev\": null, "
    "\"pdf\": \".loom/build/F7RS/premises-verbose/main.pdf\", \"built\": true, \"reused\": false, \"seconds\": <n, "
    "\"problems\": [${f7rs_problem}]}\n")
run(0 out err "${LOOM}" build F7RS --mode premises --verbose --project "${demo}" --json)
expect_match("reused" "${out}" "\"built\": true, \"reused\": true, ")

# --rev: the commit's version, in its own folder.
run(0 out err "${LOOM}" build C4JK --rev HEAD --project "${demo}" --json)
expect_match("a revision's JSON" "${out}"
    "^{\"target\": \"C4JK\", \"mode\": \"alone\", \"verbose\": false, \"rev\": \"${head}\", \"pdf\": \"\\.loom/rev/${head}/build/C4JK/alone/main\\.pdf\", \"built\": true, ")

# 1: LaTeX errors, at their source line.
apply_overlay(latex-error)
run(1 out err "${LOOM}" build K3AB --project "${demo}")
expect_equal("the error" "${err}" "Statements/K3AB.tex:13: error: Missing } inserted.\n")
expect_equal("the summary" "${out}" "Not built: 1 error, 0 warnings.\n")
run(1 out err "${LOOM}" build K3AB --project "${demo}" --json)
expect_match("a failed build's JSON" "${out}"
    "^{\"target\": \"K3AB\", .* \"built\": false, \"reused\": false, .*\"problems\": \\[{\"severity\": \"error\", \"code\": \"tex-error\", \"file\": \"Statements/K3AB.tex\", \"line\": 13, ")

# Builds wrote only under .loom/: the working tree is as the overlays left it.
run(0 out err "${GIT}" -C "${demo}" status --porcelain --untracked-files=all)
expect_equal("git status" "${out}"
    " M Papers/SmithJones24/notes.md\n"
    " M Proofs/G8TV/G8TV.a.checks.toml\n"
    " M Proofs/G8TV/G8TV.a.tex\n"
    " M Statements/C4JK.tex\n"
    " M Statements/K3AB.tex\n"
    "?? Statements/M7QR.tex\n")

message("build CLI: passed")
