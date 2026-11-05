# How-to

Step-by-step procedures for working on Loom: building, testing, releasing. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
| Page | Summary |
| --- | --- |
| [How to build and test Loom on Windows](build-on-windows.md) | Install Visual Studio 2026 (C++ workload) and Qt 6.11.1 (MSVC 2022 64-bit), point `CMAKE_PREFIX_PATH` at Qt, then in a Developer PowerShell run `cmake --preset windows`, `cmake --build --preset windows --config Debug` and `ctest --preset windows -C Debug`. You end up with `loom.exe` and `LoomApp.exe` in `build\windows\bin\Debug`. |
| [How to fix what `loom validate` reports](fix-validation-problems.md) | Run `loom validate` (or read what the refused commit printed), find each problem's code in the tables below, and fix the file and line it names; errors block a commit, warnings don't. With `--json` every problem carries its `code`. After a clone, run `loom install-hooks` so commits are checked. |
| [How to install what the Loom app needs on Windows](install-what-the-app-needs-on-windows.md) | Install Git for Windows (with Git LFS ticked), tell git your name and email, install TeX Live 2025 or newer (it brings latexmk, Perl, pdfLaTeX, BibTeX and synctex), and put Loom's folder on PATH; then *Check my setup* (on the start screen, or in Settings) shows a tick for each. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
