---
type: how-to
tags: [build, windows, cmake, qt, msvc]
---

# How to build and test Loom on Windows

> **Summary:** Install Visual Studio 2026 (C++ workload) and Qt 6.11.1 (MSVC 2022 64-bit), point `CMAKE_PREFIX_PATH` at Qt, then in a Developer PowerShell run `cmake --preset windows`, `cmake --build --preset windows --config Debug` and `ctest --preset windows -C Debug`. You end up with `loom.exe` and `LoomApp.exe` in `build\windows\bin\Debug`.

**Last verified:** 2026-11-03 · commit 7580298

## Before you start
- **Windows 11, x64**, and **Git for Windows**.
- **Visual Studio 2026** (Community is enough) with the **Desktop development with C++** workload. It brings the MSVC compiler and its own CMake (4.2 or newer, which Loom needs for the `Visual Studio 18 2026` generator).
- **Qt 6.11.1** from the [Qt Online Installer](https://www.qt.io/download-qt-installer) (it needs a free Qt account). Under *Qt → Qt 6.11.1*, tick **MSVC 2022 64-bit**; nothing else is needed for now. Qt's MSVC 2022 build works with Visual Studio 2026's compiler. The default location is `C:\Qt`, which the commands below assume; if you chose another, use yours.

## Steps
All commands are PowerShell, run from the Loom folder.

1. Open **Developer PowerShell for VS 2026** from the Start menu. It puts `cmake`, `ctest` and the compiler on `PATH`. Check the CMake version is 4.2 or newer:
   ```powershell
   cmake --version
   ```
2. Tell CMake where Qt is. Once, for your user account (then open a new Developer PowerShell):
   ```powershell
   [Environment]::SetEnvironmentVariable("CMAKE_PREFIX_PATH", "C:\Qt\6.11.1\msvc2022_64", "User")
   ```
   Or just for this window:
   ```powershell
   $env:CMAKE_PREFIX_PATH = "C:\Qt\6.11.1\msvc2022_64"
   ```
3. Get the code, if you haven't:
   ```powershell
   git clone https://github.com/DakotaMath/Loom.git
   cd Loom
   ```
4. Configure. The first time, this downloads toml++ and Catch2 (pinned, checked by SHA-256):
   ```powershell
   cmake --preset windows
   ```
5. Build (use `Release` instead of `Debug` for an optimised build, here and in the next steps):
   ```powershell
   cmake --build --preset windows --config Debug
   ```
6. Run the tests. They find Qt's DLLs on their own:
   ```powershell
   ctest --preset windows -C Debug
   ```
7. Run the programs. Outside `ctest` they need Qt's `bin` folder on `PATH`, here for this window only:
   ```powershell
   $env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;$env:PATH"
   .\build\windows\bin\Debug\loom.exe --version
   .\build\windows\bin\Debug\LoomApp.exe
   ```

## Check it worked
- `ctest` ends with `100% tests passed`.
- `loom.exe --version` prints `loom 0.1.0`.
- `LoomApp.exe` opens an empty window titled **Loom**.

## If it goes wrong
- **`Could not find a package configuration file provided by "Qt6"`:** `CMAKE_PREFIX_PATH` isn't set in this window, or doesn't point at the `msvc2022_64` folder (the one holding `bin` and `lib`). Fix it, then configure again.
- **`Can't find Qt6Core.dll in …`:** the same: `CMAKE_PREFIX_PATH` points at a Qt that isn't the MSVC 64-bit build.
- **`Could not create named generator Visual Studio 18 2026`:** an older CMake comes first on `PATH`. `(Get-Command cmake).Source` shows which; use the Developer PowerShell, or install CMake 4.2 or newer.
- **`Qt6Cored.dll was not found`, or `no Qt platform plugin could be initialized`, when running `loom.exe` or `LoomApp.exe`:** Qt's `bin` folder isn't on `PATH`, or another program's Qt comes before it. Do step 7's `$env:PATH` line in this window.
- **A changed `CMAKE_PREFIX_PATH` has no effect:** CMake remembers where it found Qt. Delete the build folder and configure again: `Remove-Item -Recurse -Force build\windows`.

## See also
- Up: [How-to](README.md)
- [001: Qt 6](../decisions/001-qt-6.md), [002: toml++](../decisions/002-tomlplusplus.md), [003: Catch2](../decisions/003-catch2.md)
- [`Specs/m1-core.md`](../../Specs/m1-core.md): *Toolchain*
