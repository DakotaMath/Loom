---
type: gotcha
tags: [windows, terminal, cli, dakota-clicks]
---

# On Windows, `_isatty()` says `NUL` is a terminal

> **Summary:** `_isatty(_fileno(stdin))` is true whenever input is a character device, and on Windows `NUL` is one, so `loom … < NUL` would pass a "only at a terminal" guard. To tell whether a person can type, ask for the console mode instead: `GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)` succeeds only for a real console. `loom`'s `inputIsTerminal()` (`Source/LoomCli/Cli.cpp`) does this.

**Last verified:** 2026-11-04 · commit 0a9d224

## Symptom
Python's `os.isatty`, which calls `_isatty` on Windows, shows it:
```
> cmd /c 'python -c "import os; print(os.isatty(0))" < NUL'
True
> cmd /c 'echo yes | python -c "import os; print(os.isatty(0))"'
False
```
A guard built on it lets an agent run Dakota's subcommands (`add-check --by dakota`, `--outdate-checks`) by redirecting input from `NUL`, and with no input the prompt reads end of input at once.

## Cause
Microsoft's documentation for `_isatty` says it reports whether the handle is a *character device*: a console, a printer or a serial port. `NUL` is a character device too.

## Fix
On Windows, use `GetConsoleMode`, which fails for anything that isn't a console (a pipe, a file, `NUL`); elsewhere `isatty(STDIN_FILENO)` is right. When it is a console, read with `ReadConsoleW` so typed non-ASCII text arrives intact, whatever the console's code page.

Not yet checked: Git Bash's default terminal, mintty, is known to give programs pipes rather than a console, in which case `loom` run there sees no terminal and refuses Dakota's subcommands. Windows Terminal, PowerShell and `cmd` give a console. Update this page once it's been tried.

## See also
- Up: [Gotchas](../README.md)
- [`Specs/m1-core.md`](../../../Specs/m1-core.md), "Decisions this spec makes": Dakota's clicks in the `loom` command
