---
type: how-to
tags: [windows, setup, git, git-lfs, tex-live, latexmk, app]
---

# How to install what the Loom app needs on Windows

> **Summary:** Install Git for Windows (with Git LFS ticked), tell git your name and email, install TeX Live 2025 or newer (it brings latexmk, Perl, pdfLaTeX, BibTeX and synctex), and put Loom's folder on PATH; then *Check my setup* (on the start screen, or in Settings) shows a tick for each.

**Last verified:** 2026-11-05 · commit d81778a

## Before you start
- Windows 11, x64, and an account that can install programs.
- About 8 GB of disk space for a full TeX Live; less with a smaller scheme (step 3).
- Loom itself: `LoomApp.exe` and `loom.exe` in one folder (a build puts them in `build\windows\bin\Release`, or `Debug`; see [How to build Loom on Windows](build-on-windows.md)).

## Steps
1. **Git for Windows** from [git-scm.com](https://git-scm.com/download/win). In the installer, keep **Git LFS (Large File Support)** ticked; the defaults are fine otherwise. Loom needs git 2.40 or newer. Check in a new PowerShell:
   ```powershell
   git --version
   git lfs version
   ```
2. **Your name and email for git**, which every commit records:
   ```powershell
   git config --global user.name "Your Name"
   git config --global user.email you@example.com
   ```
   Don't set `core.hooksPath` globally: a new project's commit guard would land in that shared folder, and `loom new-project` refuses to.
3. **TeX Live** from [tug.org/texlive](https://tug.org/texlive/windows.html): run `install-tl-windows.exe`. The full scheme is simplest. A smaller one works if it includes `latexmk`, `amsmath`, `amsfonts` and BibTeX. TeX Live brings its own Perl, which `latexmk` needs, and `synctex`. The installer puts `C:\texlive\<year>\bin\windows` on PATH. **MiKTeX** works too, but it has no Perl: install [Strawberry Perl](https://strawberryperl.com/) as well, or `latexmk` won't start.
4. **Loom's folder on PATH**, because the commit guard (the pre-commit hook) runs `loom` from PATH. In PowerShell, for your account:
   ```powershell
   [Environment]::SetEnvironmentVariable("Path", [Environment]::GetEnvironmentVariable("Path", "User") + ";C:\Path\To\Loom", "User")
   ```
   Programs started after this see it; restart Loom and your terminals.
5. Start Loom and click **Check my setup** on the start screen (or under the gear, Settings). It checks git, your git identity, Git LFS, `loom` on PATH, `latexmk`, pdfLaTeX, `synctex`, and a test build of a two-line document.

## Check it worked
- *Check my setup* shows a tick, a version and a path on every line. `synctex` missing is only a warning: clicking in the PDF can't find the source, and everything else works.
- **New project…** on the start screen creates a project and opens it.

## If it goes wrong
- **A tool is installed but shows a cross:** Loom finds tools on the PATH it started with. Restart Loom after changing PATH. For an unusual install, give the full path to `latexmk`, `synctex` or `git` in Settings; a full path overrides PATH.
- **`latexmk` shows a cross that says it exited with an error, with MiKTeX:** Perl is missing (step 3).
- **The test build fails:** the lines shown are from LaTeX's log. A missing package means a smaller TeX Live scheme lacks it; add it with `tlmgr install <package>` in an administrator PowerShell.
- **The test build runs out of time:** the first build after installing can be slow while TeX builds its font caches. Run *Check my setup* again, or raise the compile timeout in Settings.
- **git identity shows a cross:** do step 2; the line repeats the commands.

## See also
- Up: [How-to](README.md)
- [How to build Loom on Windows](build-on-windows.md): building `LoomApp.exe` and `loom.exe`
- [`Specs/m2-statements-review.md`](../../Specs/m2-statements-review.md), *Settings*: what *Check my setup* checks
