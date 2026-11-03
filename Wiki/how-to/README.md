# How-to

Step-by-step procedures for working on Loom: building, testing, releasing. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
| Page | Summary |
| --- | --- |
| [How to build and test Loom on Windows](build-on-windows.md) | Install Visual Studio 2026 (C++ workload) and Qt 6.11.1 (MSVC 2022 64-bit), point `CMAKE_PREFIX_PATH` at Qt, then in a Developer PowerShell run `cmake --preset windows`, `cmake --build --preset windows --config Debug` and `ctest --preset windows -C Debug`. You end up with `loom.exe` and `LoomApp.exe` in `build\windows\bin\Debug`. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
