# M3 Usermode Injection Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the WH_GETMESSAGE global hook that injects GameProtect.dll into every UI process. Replace with targeted injection into the game process only, triggered from HerculesAC after IOCTL_START_PROTECT, plus a kernel thread-creation notification for future kernel-APC upgrade.

**Architecture:** GameMon no longer calls SetupMsgHook / UnHookMsgHook. Instead, HerculesAC.exe calls InjectDll(pid, path) using CreateRemoteThread + LoadLibraryW immediately after StartProtect(pid) succeeds. The kernel registers PsSetCreateThreadNotifyRoutine for future APC injection groundwork. IsWhitelistCurrentProcess in GameProtect is removed — with targeted injection, GameProtect only loads inside the game process.

**Tech Stack:** WinAPI (CreateRemoteThread, VirtualAllocEx, WriteProcessMemory, LoadLibraryW), WDM (PsSetCreateThreadNotifyRoutine), C++17 MSVC, WDK 10.0.26100

## Global Constraints

- Kernel driver must be built with VS 2022 MSBuild only (WDK 26100 `DriverKit.Build.Tasks.17.0`; VS 2026 MSBuild 18 incompatible)
- No new external library dependencies — only WinAPI / WDK APIs
- GameProtect.dll exports `SetupMsgHook`, `UnHookMsgHook`, `GetMsgProc` are DELETED — update callers at the same time
- Private keys never committed; only public key hex may appear in source
- Builds must pass Debug x64 with zero errors (pre-existing LNK4099 PDB warnings are acceptable)
- All kernel pool allocations use `ExAllocatePool2` (not deprecated `ExAllocatePoolWithTag`)
- Commit message format: `feat/chore(scope): description` + `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>` + `Claude-Session: https://claude.ai/code/session_011gJWcQS8FUmGu9NKctEud4`

---

### Task 1: Remove WH_GETMESSAGE hook exports from GameProtect + callers in GameMon

**Files:**
- Modify: `GameProtect/Hook/MsgHook/MsgHook.cpp`
- Modify: `GameProtect/Hook/MsgHook/MsgHook.h`
- Modify: `GameProtect/dllmain.cpp`
- Modify: `GameMon/WinMain.h`
- Modify: `GameMon/WinMain.cpp`

**Interfaces:**
- Produces: `GameProtect.dll` no longer exports `SetupMsgHook`, `UnHookMsgHook`, `GetMsgProc`
- Produces: `GameMon` no longer calls those exports; still loads `GameProtect.dll` for `GetSharedMemoryPtr`

- [ ] **Step 1: Stub MsgHook.cpp to no-ops (keep the file for build compatibility)**

Replace the entire content of `GameProtect/Hook/MsgHook/MsgHook.cpp` with:
```cpp
// WH_GETMESSAGE global hook removed in M3 (targeted injection via CreateRemoteThread).
// File kept as a stub so build system references compile cleanly.
```

- [ ] **Step 2: Clear MsgHook.h exports**

Replace `GameProtect/Hook/MsgHook/MsgHook.h` with:
```cpp
#pragma once
// WH_GETMESSAGE global hook removed in M3.
```

- [ ] **Step 3: Remove MsgHook include from GameProtect/dllmain.cpp**

In `GameProtect/dllmain.cpp` line 2, remove:
```cpp
#include "Hook/MsgHook/MsgHook.h"
```

- [ ] **Step 4: Update GameMon/WinMain.h — remove hook function pointer typedefs**

Remove these two lines from `GameMon/WinMain.h`:
```cpp
typedef void(* PFN_SETUPMSGHOOK)(HINSTANCE hinstDLL);
typedef void(* PFN_UNHOOKMSGHOOK)();
```
Keep `typedef SharedMemory* (* PFN_GETSHAREDMEMORYPTR)();` and `PFN_ADDRESS`.

- [ ] **Step 5: Update GameMon/WinMain.cpp — remove all hook usage**

In `GameMon/WinMain.cpp`:
a) Remove the two global declarations:
```cpp
PFN_SETUPMSGHOOK SetupMsgHook;
PFN_UNHOOKMSGHOOK UnHookMsgHook;
```

b) Replace `SetupHook()`:
```cpp
void SetupHook()
{
    // Global WH_GETMESSAGE hook removed in M3.
    // GameProtect.dll is now injected by HerculesAC.exe via CreateRemoteThread.
}
```

c) Replace `UnHook()`:
```cpp
void UnHook()
{
    // No-op: hook removal handled by DLL_PROCESS_DETACH in GameProtect.
}
```

d) In `InitFunctionPtr()`, remove the two `GetProcAddress` calls for `SetupMsgHook` / `UnHookMsgHook`. Keep only:
```cpp
void InitFunctionPtr()
{
#ifdef _WIN64
    Global::hGameProtect = LoadLibrary(L"./GameProtect64.dll");
#else
    Global::hGameProtect = LoadLibrary(L"./GameProtect.dll");
#endif
    GetSharedMemoryPtr = (PFN_GETSHAREDMEMORYPTR)GetProcAddress(Global::hGameProtect, "GetSharedMemoryPtr");
}
```

- [ ] **Step 6: Build GameMon Debug x64 — verify zero errors**

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" GameMon\GameMon.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```
Expected: `GameMon.exe` produced, zero errors.

- [ ] **Step 7: Build GameProtect Debug x64 — verify zero errors**

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" GameProtect\GameProtect.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```
Expected: `GameProtect64.dll` produced, zero errors.

- [ ] **Step 8: Commit**

```
git add GameProtect/Hook/MsgHook/MsgHook.cpp GameProtect/Hook/MsgHook/MsgHook.h
git add GameProtect/dllmain.cpp GameMon/WinMain.h GameMon/WinMain.cpp
git commit -m "feat(m3): remove WH_GETMESSAGE global hook surface from GameProtect + GameMon"
```

---

### Task 2: Targeted injection in HerculesAC (CreateRemoteThread + LoadLibraryW)

**Files:**
- Create: `HerculesAC/Inject/ProcessInjector.h`
- Create: `HerculesAC/Inject/ProcessInjector.cpp`
- Modify: `HerculesAC/WinMain.h` (add include)
- Modify: `HerculesAC/WinMain.cpp` (wire after StartProtect)
- Modify: `HerculesAC/HerculesAC.vcxproj` (add new files)

**Interfaces:**
- Produces: `hac::inject::InjectDll(DWORD pid, const WCHAR* dllPath) -> bool`
- Consumes: `hac::driver::StartProtect(pid)` from Task 9 of M2 (already in WinMain.cpp)

- [ ] **Step 1: Create ProcessInjector.h**

File: `HerculesAC/Inject/ProcessInjector.h`
```cpp
#pragma once
#include <Windows.h>

namespace hac { namespace inject {

// Opens the process by pid, maps dllPath into its address space, and
// creates a remote thread that calls LoadLibraryW(dllPath).
// Returns true on success; false if the process cannot be opened,
// memory cannot be allocated, or the remote thread fails to start.
// Non-fatal: logs failure but does not kill the game.
bool InjectDll(DWORD pid, const WCHAR* dllPath);

}} // namespace hac::inject
```

- [ ] **Step 2: Create ProcessInjector.cpp**

File: `HerculesAC/Inject/ProcessInjector.cpp`
```cpp
#include "ProcessInjector.h"
#include <cstring>

namespace hac { namespace inject {

bool InjectDll(DWORD pid, const WCHAR* dllPath)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD,
        FALSE, pid);
    if (!hProcess)
        return false;

    size_t pathBytes = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    LPVOID remote = VirtualAllocEx(hProcess, nullptr, pathBytes,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remote, dllPath, pathBytes, nullptr)) {
        VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    FARPROC loadLib = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!loadLib) {
        VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLib),
        remote, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Wait up to 10 s for LoadLibraryW to complete, then clean up.
    WaitForSingleObject(hThread, 10000);
    VirtualFreeEx(hProcess, remote, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}

}} // namespace hac::inject
```

- [ ] **Step 3: Wire injection in HerculesAC/WinMain.h**

After the existing `#include "Ioctl/DriverClient.h"` line, add:
```cpp
#include "Inject/ProcessInjector.h"
```

- [ ] **Step 4: Wire injection in HerculesAC/WinMain.cpp — InitProcess()**

In `InitProcess()`, after the existing `StartProtect` call, add the injection.
Find this block (from M2 Task 9):
```cpp
        pi = StartProcess(Global::GamePath, config[L"starter"], L"");

        if (pi.dwProcessId != 0)
            hac::driver::StartProtect(pi.dwProcessId);
```

Replace with:
```cpp
        pi = StartProcess(Global::GamePath, config[L"starter"], L"");

        if (pi.dwProcessId != 0) {
            hac::driver::StartProtect(pi.dwProcessId);

            std::wstring modDir = FileSystem::GetModuleDirectory(NULL);
#ifdef _WIN64
            std::wstring dllPath = modDir + L"GameProtect64.dll";
#else
            std::wstring dllPath = modDir + L"GameProtect.dll";
#endif
            hac::inject::InjectDll(pi.dwProcessId, dllPath.c_str());
        }
```

- [ ] **Step 5: Add new files to HerculesAC.vcxproj**

In `HerculesAC/HerculesAC.vcxproj`, add to the ClCompile ItemGroup (after `Ioctl\DriverClient.cpp`):
```xml
<ClCompile Include="Inject\ProcessInjector.cpp" />
```

Add to the ClInclude ItemGroup (after `Ioctl\DriverClient.h`):
```xml
<ClInclude Include="Inject\ProcessInjector.h" />
```

- [ ] **Step 6: Build HerculesAC Debug x64 — verify zero errors**

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" HerculesAC\HerculesAC.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```
Expected: `HerculesAC.exe` produced, zero errors.

- [ ] **Step 7: Commit**

```
git add HerculesAC/Inject/ProcessInjector.h HerculesAC/Inject/ProcessInjector.cpp
git add HerculesAC/WinMain.h HerculesAC/WinMain.cpp HerculesAC/HerculesAC.vcxproj
git commit -m "feat(m3): targeted injection via CreateRemoteThread+LoadLibraryW in HerculesAC"
```

---

### Task 3: Kernel PsSetCreateThreadNotifyRoutine (monitoring groundwork)

**Files:**
- Modify: `herculeskernel/Protect/Callback/Callbacks.cpp`
- Modify: `herculeskernel/Protect/Callback/Callbacks.h`
- Modify: `herculeskernel/Driver.cpp`

**Interfaces:**
- Produces: `InitThreadNotify(PDRIVER_OBJECT)` / `UninstallThreadNotify()` exported from Callbacks.h
- Consumes: `g_pidTable.Contains(ULONG)` from `herculeskernel/Globals.h`

- [ ] **Step 1: Add thread notify declarations to Callbacks.h**

Append to `herculeskernel/Protect/Callback/Callbacks.h`:
```cpp
VOID InitThreadNotify();
VOID UninstallThreadNotify();
```

- [ ] **Step 2: Implement thread notify callback in Callbacks.cpp**

At the end of `Callbacks.cpp`, add:
```cpp
static VOID OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
    UNREFERENCED_PARAMETER(ThreadId);
    if (!Create)
        return;
    ULONG pid = static_cast<ULONG>((ULONG_PTR)ProcessId);
    if (g_pidTable.Contains(pid)) {
        outLog("ThreadNotify: new thread in protected PID %lu", pid);
    }
}

VOID InitThreadNotify()
{
    NTSTATUS status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
    if (!NT_SUCCESS(status)) {
        outLog("PsSetCreateThreadNotifyRoutine failed: 0x%08X", status);
    }
}

VOID UninstallThreadNotify()
{
    PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
}
```

- [ ] **Step 3: Wire InitThreadNotify / UninstallThreadNotify in Driver.cpp**

In `InitProtect`:
```cpp
VOID InitProtect(IN PDRIVER_OBJECT pDriver_Object)
{
    SetThreadCallbacks(pDriver_Object);
    SetProcessCallbacks(pDriver_Object);
    InitThreadNotify();
}
```

In `UninstallProtect`:
```cpp
VOID UninstallProtect()
{
    UninstallThreadNotify();
    UnThreadCallbacks();
    UnProcessCallbacks();
}
```

Note: unregister thread notify BEFORE Ob callbacks to avoid a race where OnThreadNotify fires during Ob teardown.

- [ ] **Step 4: Build herculeskernel Debug x64 — verify zero errors**

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" herculeskernel\herculeskernel.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```
Expected: `herculeskernel.sys` produced, signed, zero errors.

- [ ] **Step 5: Commit**

```
git add herculeskernel/Protect/Callback/Callbacks.h herculeskernel/Protect/Callback/Callbacks.cpp
git add herculeskernel/Driver.cpp
git commit -m "feat(kernel): add PsSetCreateThreadNotifyRoutine monitoring for protected PIDs"
```

---

### Task 4: Remove IsWhitelistCurrentProcess from GameProtect (dead code with targeted injection)

**Files:**
- Modify: `GameProtect/dllmain.cpp`

**Interfaces:**
- Produces: `InitHook()` unconditionally calls `SetupHook()` — no whitelist check

- [ ] **Step 1: Remove IsWhitelistCurrentProcess function and its callers**

In `GameProtect/dllmain.cpp`:

a) Remove the entire `IsWhitelistCurrentProcess()` function (lines 72–113 in the original).

b) Replace `InitHook()`:
```cpp
void InitHook()
{
    SetupHook();
}
```

c) The `#include <sodium.h>` and `#include <fstream>` were only needed by `IsWhitelistCurrentProcess`.
   Keep `<sodium.h>` if anything else uses it; remove `<fstream>` if nothing else opens files.
   Inspect the file — if `<fstream>` is now unused, remove it.

- [ ] **Step 2: Build GameProtect Debug x64 — verify zero errors**

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" GameProtect\GameProtect.vcxproj /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- [ ] **Step 3: Commit**

```
git add GameProtect/dllmain.cpp
git commit -m "feat(m3): remove IsWhitelistCurrentProcess from GameProtect — targeted injection makes it dead code"
```

---

### Task 5: E2E test doc, README refresh, and milestone tag

**Files:**
- Create: `docs/testing/m3-e2e-injection.md`
- Modify: `README.md` (update Known Issues, Recent History)

- [ ] **Step 1: Create docs/testing/m3-e2e-injection.md**

```markdown
# M3 end-to-end injection test

Objective: prove that GameProtect.dll is loaded only in the game process,
not globally injected into every UI process.

## Prerequisites
- Test-signed kernel driver loaded (see m2-e2e-driver-load.md)
- Admin rights

## 1. Build
Builder.bat

## 2. Verify GameProtect.dll is no longer globally injected
1. Open Process Hacker or Task Manager.
2. Launch a random UI app (Notepad.exe).
3. Check Notepad's module list — GameProtect.dll must NOT appear.

## 3. Launch the game via HerculesAC.exe
HerculesAC.exe <GamePath>

## 4. Verify GameProtect.dll is in the game process only
1. In Process Hacker, open the starter process (cstrike.exe or equivalent).
2. Go to Modules tab.
3. Confirm GameProtect.dll (or GameProtect64.dll) appears in the module list.
4. Confirm no other non-game UI process has GameProtect loaded.

## 5. Verify Ob callbacks still work
From a second process, attempt OpenProcess(PROCESS_VM_READ, FALSE, <game_pid>).
Expected: PROCESS_VM_READ is stripped (access denied on ReadProcessMemory).

## 6. Defender check
On a stock Windows 11 24H2 VM with Defender fully enabled, complete steps 3-5.
Expected: Windows Security Center shows zero new alerts.
```

- [ ] **Step 2: Update README Known Issues (remove WH_GETMESSAGE global injection entry if present)**

Search README.md for any mention of global hook / WH_GETMESSAGE and update.
Add M3 Injection Redesign to Recent History section.

- [ ] **Step 3: Commit all docs + tag**

```
git add docs/testing/m3-e2e-injection.md README.md
git commit -m "docs(m3): E2E injection test doc + README update"
git tag -a m3-injection-redesign -m "M3 Injection Redesign complete — global hook removed, targeted CreateRemoteThread injection, PsCreateThread notify"
```
