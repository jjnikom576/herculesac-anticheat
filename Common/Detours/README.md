# Microsoft Detours

**Version:** 4.0.1
**Source:** https://github.com/microsoft/Detours (tag v4.0.1, commit e4bfd6b03e50de46b47abfbd1e46b384f0c5f833)
**License:** MIT (see the upstream repo)
**Variant:** static lib, /MT, v143 toolset, Windows SDK

Only the `detours.h` header (already vendored) plus the two static `.lib`
files (`x64/detours.lib`, `x86/detours.lib`) are committed. The upstream
source tree is not — rebuild via the upstream instructions if upgrading.

`Common/Detours/Hook.cpp` links these via `#pragma comment(lib, …)`.

## Build Notes

Built with:
- Visual Studio 2022 Community (v143 toolset)
- nmake with DETOURS_TARGET_PROCESSOR=X64 and DETOURS_TARGET_PROCESSOR=X86
- /MT static runtime (matches project configuration)
- Windows SDK for vcvars64.bat and vcvarsamd64_x86.bat

File sizes:
- x64/detours.lib: 769044 bytes
- x86/detours.lib: 623412 bytes
