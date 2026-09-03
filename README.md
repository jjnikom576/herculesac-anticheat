# Hercules Anti-Cheat

A modular Windows anti-cheat framework built for older MMO / FPS clients
(the packaged build targets `cstrike.exe`).  The project pairs a user-mode
watchdog + injected hook DLL with an optional kernel-mode handle-stripping
driver, all bound together with VMProtect virtualization and a small
INI/MD5 manifest that ties the shipped binaries together.

> **Disclaimer.**  This repository is intended for research and educational
> use — reverse-engineering, kernel programming, hook development and
> anti-cheat design.  It is **not** a production-ready product, ships with
> hard-coded debug values (see *Known issues* below), and must not be
> deployed on any system you do not own.  A number of techniques used
> here (global message hooks, kernel `Ob*` callbacks, VMProtect) are
> heavy-handed and will trip real EDR/AV products.

---

## Table of contents

1. [Component overview](#component-overview)
2. [Runtime architecture](#runtime-architecture)
3. [Detection techniques](#detection-techniques)
4. [Configuration file — `hac.dat`](#configuration-file--hacdat)
5. [Repository layout](#repository-layout)
6. [Third-party code](#third-party-code)
7. [Building](#building)
8. [Deployment layout](#deployment-layout)
9. [Known issues / TODO](#known-issues--todo)
10. [Recent history](#recent-history)

---

## Component overview

The Visual Studio solution (`HerculesAC.sln`, VS 2022, platform toolset
v143) contains seven projects grouped into a **user-mode** folder and a
**kernel-mode** folder.

| Project          | Kind         | Arch     | Role |
| ---------------- | ------------ | -------- | ---- |
| `GameLauncher`   | `.exe`       | x64      | Thin bootstrapper the player double-clicks. Locates its own directory and spawns `HerculesAC\HerculesAC.aes` (post-VMProtect binary), passing the game path on the command line. |
| `HerculesAC`     | `.exe`       | x64      | The watchdog.  Reads `hac.dat`, launches the game client, spawns both `GameMon` bitness variants, self-hooks `BaseThreadInitThunk` + `CreateThread` for self-defense, scans the process list / windows / resource icons / version strings for known hack tools, and CRC32-verifies its own hooks. |
| `GameMon`        | `.exe`       | x86 + x64 | Per-bitness monitor.  Loads `GameProtect(64).dll`, opens shared memory and publishes the game PID / paths for the injected DLLs to consume. |
| `GameProtect`    | `.dll`       | x86 + x64 | The globally-injected hook DLL.  `SetWindowsHookEx(WH_GETMESSAGE, …)` from `GameMon` causes Windows to broadcast this DLL into every UI process on the desktop.  In `DllMain` it reads the shared memory, then Detours-hooks `ReadProcessMemory` / `WriteProcessMemory`.  Any call whose target PID equals the protected game PID triggers termination logic. |
| `Bypass`         | `.dll`       | x86 + x64 | Standalone experiment showing tricks to *fool* the checks — hides `OLLYDBG` from `FindWindowW`, freezes a CRC32 function via `Sleep(-1)`, contains an x64 MASM stub (`Asm\AsmCallset.asm`).  Not loaded by the shipping product; kept for research. |
| `ExampleDLL`     | `.dll`       | x86 + x64 | Minimal “Hello DLL” used to test the DLL-injection paths. |
| `herculeskernel` | `.sys` (KMDF-style WDM) | x64 + ARM64 | Optional ring-0 driver.  Registers `PsProcessType` / `PsThreadType` `ObRegisterCallbacks` at altitude `321000` and strips `PROCESS_VM_READ`, `PROCESS_VM_WRITE` and `THREAD_SUSPEND_RESUME` from any handle opened against the protected PID. |

All user-mode projects are compiled as Unicode (`_UNICODE`), MFC-free
Win32 (`WinMain`), and statically link against the local `Common\` code
and Microsoft Detours (`Common\Detours\{x86,x64}\detours.lib`).

## Runtime architecture

```
                            ┌──────────────────┐
    player double-clicks →  │ GameLauncher.exe │  (x64, ~35 LOC)
                            └────────┬─────────┘
                                     │ CreateProcess
                                     ▼
                            ┌──────────────────┐
                            │  HerculesAC.aes  │  (VMProtect-packed
                            │  (HerculesAC.exe)│   HerculesAC.exe)
                            └────────┬─────────┘
                                     │
             ┌───────────────────────┼──────────────────────────┐
             │                       │                          │
             ▼                       ▼                          ▼
   ┌──────────────────┐    ┌──────────────────┐      ┌──────────────────┐
   │  cstrike.exe     │    │  GameMon.aes     │      │  GameMon64.aes   │
   │  (game client)   │    │  (x86 monitor)   │      │  (x64 monitor)   │
   └──────────────────┘    └────────┬─────────┘      └────────┬─────────┘
                                    │ LoadLibrary               │
                                    ▼                           ▼
                          ┌──────────────────┐        ┌──────────────────┐
                          │ GameProtect.dll  │        │ GameProtect64.dll│
                          │  SetWindowsHookEx│        │  SetWindowsHookEx│
                          │  (WH_GETMESSAGE) │        │  (WH_GETMESSAGE) │
                          └────────┬─────────┘        └────────┬─────────┘
                                   │  Windows broadcasts the DLL into
                                   │  every UI process that receives a
                                   ▼  message from the hook chain
                          ┌────────────────────────────────────────────┐
                          │  DllMain in every UI process:              │
                          │   • read hac.dat / whitelist self by MD5   │
                          │   • open named shared memory (game PID)    │
                          │   • Detours-hook ReadProcessMemory /       │
                          │     WriteProcessMemory                     │
                          │   • if caller targets the game PID and     │
                          │     exceeds a rate threshold → terminate   │
                          │     the game (deny-by-suicide)             │
                          └────────────────────────────────────────────┘

           (optional, ring-0)
   ┌───────────────────────────┐
   │  herculeskernel.sys       │  Ob* callbacks strip VM_READ/WRITE
   │  altitude 321000          │  and SUSPEND_RESUME from any handle
   └───────────────────────────┘  opened against the protected PID
```

### Launch sequence (annotated)

1. **`GameLauncher::WinMain`** — determines its own directory and calls
   `StartProcess(dir, L"HerculesAC\\HerculesAC.aes", L" " + dir)`.  The
   `.aes` extension is the VMProtect-packed twin of `HerculesAC.exe`;
   the batch scripts run VMProtect on every shipping binary and rename
   the output.  (See `vmp.bat`.)
2. **`HerculesAC::WinMain`** — takes a singleton lock (`HACMutexName`
   mutex), shows the splash bitmap `assets\hac.bmp` for 5 s, then:
   - `LoadConfig` parses `hac.dat` and asserts every referenced file
     is present in the game / anti-cheat directories.
   - `InitFunctionPointer` resolves `BaseThreadInitThunk`,
     `RtlExitUserThread`, `CreateThread`.
   - `SetupHook` Detours-hooks `BaseThreadInitThunk` and
     `CreateThread` for self-defense.
   - `InitCheckSum` snapshots CRC32 of the first 5 bytes of
     `FindWindowW`, `BaseThreadInitThunk` and `CreateThread` (inline
     patch detection).
   - `InitThread` spawns two detached scanners:
     `DetectIllegalActivities` (CRC integrity, 100 ms) and
     `ScannerThread` (icon/version signature scan, 100 ms).
   - `InitProcess` starts the game client, then launches both
     `GameMon` variants with a JSON payload
     `{"GamePath": …, "pid": …, "hProcess": …}` on their command
     lines.
   - `StartServer` polls every 5 s: if the game process is gone the
     loop exits; if a known bad process / window shows up it kills
     the game (`ExitGameProcess`).
3. **`GameMon::WinMain`** — parses the JSON, singleton-locks
   (`GameMonMutexName` / `GameMonMutexName64`), loads
   `GameProtect(64).dll`, opens the shared memory
   (`hacSharedMemory` / `hacSharedMemory64`), writes the current
   game PID / client name / game directory into it, then calls
   `SetupMsgHook(hInst)` which installs the global `WH_GETMESSAGE`
   hook.  From that point on every UI process on the desktop that
   receives a queued message will load `GameProtect(64).dll`.
4. **`GameProtect::DllMain`** — in *every* process the hook is
   injected into: read `hac.dat`, MD5 the module file, whitelist
   self if the hash matches an entry in the `[MD5]` section,
   otherwise install Detours hooks for `ReadProcessMemory` and
   `WriteProcessMemory` and share the same `SharedMemory` pointer
   with the host via the exported `GetSharedMemoryPtr`.

### IPC — shared memory schema

Defined in `Common/IPC/SharedMemory/SharedMemory.h` and
`Common/Shared/SharedStruct.h`.  A named file-mapping is created
per-bitness (`hacSharedMemory` for 32-bit, `hacSharedMemory64` for
64-bit) with the following payload:

```c
struct GAME_CLIENT {
    DWORD dwPid;               // game process id
    TCHAR szClient[100];       // game executable file name
    TCHAR szGamePath[256];     // game directory
};

struct IPC_MSG_RCD {
    DWORD MsgId;               // IPC_GAME_CLIENT_ID = 1000
    BYTE  buffer[1024];        // GAME_CLIENT is memcpy'd here
};

struct SHARED_DATA { IPC_MSG_RCD ipc_msg; };
```

`SharedMemory` also templates `CopyToBuffer` / `parseBuffer` so other
`MsgId`s can be marshalled through the same 1 KiB slot in the future.
The critical section that would serialize readers/writers is currently
`#pragma`ed out.

## Detection techniques

The user-mode side layers several independent detectors, each running
on its own polling thread:

| Signal | Where | What it does |
| ------ | ----- | ------------ |
| Process name blacklist | `HerculesAC/WinMain.cpp` — `CheckProcessNames` | Case-insensitive substring match against `ollydbg`, `Dbgview`, `x32dbg`, `x64dbg`, `cheatengine`, `Picker`, `WPE.exe`.  Match ⇒ log + `ExitGameProcess`. |
| Window class blacklist | `HerculesAC/WinMain.cpp` — `CheckWindow` | Uses `FindWindow` for the same set of class names. |
| Icon / bitmap hash | `HerculesAC/Threads/ActiveThread.cpp` — `CheckHackTools` | Enumerates every running process, `LoadLibraryEx(LOAD_LIBRARY_AS_DATAFILE)` on the executable image, MD5s each `RT_ICON` and `RT_BITMAP` resource, matches against a table (`gh-injector`, Process Hacker, x64dbg, Cheat Engine + a “custom cult CE”, IDA).  Sample icons live in `assets/hacktools-sig/ico/*.bin`. |
| Version-info hash | `HerculesAC/Threads/ActiveThread.cpp` — `CheckVersionInfo` | Pulls `Comments`, `CompanyName`, `FileDescription`, `InternalName`, `LegalCopyright`, `OriginalFilename`, `ProductName` from every process’ VERSIONINFO resource and MD5s each field.  Matches against a table of known `OpenArk` / `GH-Injector` string hashes.  Sample dumps live in `assets/hacktools-sig/version/*.bin`. |
| Hook-code integrity | `HerculesAC/Threads/ActiveThread.cpp` — `CheckHookCode` | Re-CRC32s the first 5 bytes of `FindWindowW`, `BaseThreadInitThunk`, `CreateThread` and compares against the boot-time snapshot; mismatch ⇒ `ExitGameProcess`. |
| Suspicious thread starts | `HerculesAC/HookCallSet/functionSet.cpp` — `NewBaseThreadInitThunk` | Every user thread is registered via a `NewCreateThread` shim.  If a thread reaches `BaseThreadInitThunk` whose start routine was **not** first observed via `CreateThread` — or whose start routine is `LoadLibraryA/W` — it is force-exited via `RtlExitUserThread` and a `__debugbreak()` is left as a trap. |
| Cross-process memory access | `GameProtect/Hook/DetoursHook/HookCallSet/functionSet.cpp` | Detours on `ReadProcessMemory` and `WriteProcessMemory`.  If the target `hProcess` maps to the game PID, WPM immediately kills the game; RPM applies a rate-limit (> 1000 reads in 60 s ⇒ kill). |
| UI-level message hook | `GameProtect/Hook/MsgHook/MsgHook.cpp` | The `WH_GETMESSAGE` hook itself is the injection primitive — the callback is a plain pass-through (`CallNextHookEx`). |
| Kernel handle stripping | `herculeskernel/Protect/Callback/Callbacks.cpp` | Ring-0 `Ob*` pre-op callbacks remove `VM_READ`/`VM_WRITE` for process handles and `SUSPEND_RESUME` for thread handles targeting the protected PID.  Effectively blocks user-mode RPM/WPM and thread freezing even against callers that bypass the user-mode hooks. |

## Configuration file — `hac.dat`

An INI file generated by `calculateMD5.bat` at build time and read by
every user-mode component through `GetPrivateProfileString`.

```ini
[Game]
starter  = cstrike.exe    ; process HerculesAC actually launches
Client   = cstrike.exe    ; process HerculesAC monitors for liveness
GameMon  = GameMon.aes    ; 32-bit monitor executable
GameMon64 = GameMon64.aes ; 64-bit monitor executable

[MD5]
hash0 = <MD5 of HerculesAC.aes>
hash1 = <MD5 of GameMon.aes>
hash2 = <MD5 of GameMon64.aes>
hash3 = <MD5 of GameLauncher.exe>
hash4 = <MD5 of cstrike.exe>
Count = 5
```

`Count` is used by `GameProtect::LoadConfig` to size the whitelist:
whenever `GameProtect(64).dll` is injected into a new process it MD5s
the host module file and — if the hash appears in `[MD5]` — skips
installing the RPM/WPM hooks in that process.  This is how the
watchdog / monitor / launcher / game itself avoid hooking themselves.

The `[Game]` fallback of `starter == Client == cstrike.exe` is the
shipped default; changing the target game means editing
`calculateMD5.bat` to reference the new executable.

## Repository layout

```
herculesac-anticheat/
├── HerculesAC.sln                Visual Studio 2022 solution
│
├── GameLauncher/                 x64 bootstrapper (WinMain + resources)
├── HerculesAC/                   Main watchdog (WinMain + HookCallSet + Threads)
├── GameMon/                      x86/x64 monitor (WinMain)
├── GameProtect/                  Injected hook DLL (dllmain + Hook/*)
├── Bypass/                       Research DLL (dllmain + HookCallSet + Asm)
├── ExampleDLL/                   Minimal template DLL
├── herculeskernel/               Ring-0 driver (Driver.cpp + Protect/Callback)
│
├── Common/                       Shared usermode + kernel-mode code
│   ├── Common.{h,cpp}            String conversion, process enum, singleton, CPU vendor
│   ├── Detours/                  Microsoft Detours 4 (import lib + headers)
│   ├── Encrypt/Blowfish/         Blowfish reference implementation (user-mode copy)
│   ├── FileSystem/               INI helpers + directory traversal
│   ├── Hash/                     CRC32 + MD5 (byte / file / string overloads)
│   ├── IPC/SharedMemory/         Named file-mapping wrapper
│   ├── Logger/                   Thread-safe file + OutputDebugString logger
│   ├── Ring0/                    Kernel-only helpers (ia32-doc, Blowfish, list, string, allocator)
│   ├── Shared/                   IOCTL codes + user-mode/kernel shared structs
│   ├── VMProtectSDK*             VMProtect static libs + header
│   ├── vmp.h                     RAII wrapper (`VMProtectionScope`) around VMP begin/end
│   └── include/                  Vendored deps: jsoncpp, thread-safe vector
│
├── assets/
│   ├── hac.bmp                   Splash bitmap shown for 5 s at launch
│   ├── Originals/hac.bmp         Master copy of the splash
│   └── hacktools-sig/            Reference binaries used to derive the MD5 tables
│       ├── ico/*.bin             Raw icon dumps (CE, Cult-CE, IDA, x32/x64dbg, gh-injector, procexp)
│       └── version/*.bin         VERSIONINFO string dumps (gh-injector, OpenArk)
│
├── captures/                     Empty; reserved for runtime capture dumps
│
├── Builder.bat                   Full solution build (x86 + x64) → vmp → hac.dat
├── ReBuilder.bat                 Same as above with /t:Rebuild
├── Project.bat                   Absolute paths to each .vcxproj + build config
├── vmp.bat                       Runs VMProtect_Con.exe on the three shipped exes
├── calculateMD5.bat              Emits hac.dat with the [Game] and [MD5] tables
│
├── .gitignore                    Stock Visual Studio ignore file
└── README.md                     ← this file
```

## Third-party code

Vendored inside the repository (no package manager is used):

- **Microsoft Detours 4** — `Common/Detours/{detours.h, x86/detours.lib, x64/detours.lib}`
  drives every inline hook in the user-mode components.
- **JsonCpp** — `Common/include/json/*` — parses the JSON blob the
  watchdog passes to `GameMon` on the command line.
- **VMProtect SDK** — `Common/VMProtectSDK.h`, `VMProtectSDK32.lib`,
  `VMProtectSDK64.lib` — provides `VMProtectBegin*/End` markers that
  the standalone `VMProtect_Con.exe` (see `vmp.bat`) inflates into
  virtualized bytecode when the shipped `*.aes` files are produced.
- **ia32-doc** (`Common/Ring0/ia32-doc/`) — Intel manuals mechanically
  converted into `ia32.hpp` for the kernel driver.
- **process-hacker/phnt** (`herculeskernel/Ring0/SymbolicAccess/Phnt/`)
  — full copy of the phnt native API headers for ring-0 use.
- **SymbolicAccess** (`herculeskernel/Ring0/SymbolicAccess/`) — PDB
  reader + module-extender used by the driver to resolve
  `_EPROCESS::ImageFileName` at runtime.  Linked against
  `SymbolicAccessKM.lib` in the same directory.
- **jxy allocator** (`SymbolicAccess/Utils/Km/jxy/`) — STL-compatible
  pool allocator for kernel builds.
- **Blowfish** — two reference implementations shipped side-by-side
  (`Common/Encrypt/Blowfish` for user mode, `Common/Ring0/Encrypt/Blowfish`
  for kernel mode).

## Building

### Prerequisites

- **Visual Studio 2022** (17.9 or newer) with the *Desktop development
  with C++* workload — the solution requires the v143 platform toolset
  and the Windows 10/11 SDK.
- **Windows Driver Kit (WDK)** matching the SDK version, for
  `herculeskernel`.  The driver targets Windows 10+ (build 10240 and
  up) and is signed with a test certificate by default; enable *test
  signing* (`bcdedit /set testsigning on`) on the target VM before
  loading it.
- **VMProtect Ultimate** with `VMProtect_Con.exe` available; the path
  is set in `vmp.bat` (`..\anticheat\VMProtect Ultimate v3.3.1 …`).
- **`certutil`** (in-box on Windows) is used by `calculateMD5.bat` to
  emit MD5 hashes.
- **Administrator shell** is required — the kernel driver install and
  the global `WH_GETMESSAGE` hook usually need it.

### Batch-driven build

`Builder.bat` chains everything in the right order:

```
Project.bat        -> exports GameLauncher / HerculesAC / GameMon / GameProtect
                      vcxproj paths and buildConfig ("Debug" or "Release")
msbuild GameLauncher   /p:Platform=x64
msbuild HerculesAC     /p:Platform=x64
msbuild GameMon        /p:Platform=x86 + x64
msbuild GameProtect    /p:Platform=x86 + x64
vmp.bat            -> VMProtect_Con.exe HerculesAC.exe / GameMon.exe / GameMon64.exe
calculateMD5.bat   -> emits x64\Debug\hac.dat with [Game] + [MD5] tables
```

`ReBuilder.bat` does the same with `/t:Rebuild`.  Both call
`pause` at the end so the console does not close on error.

> **Important — fix the paths first.**  `Project.bat` and
> `calculateMD5.bat` still reference the old checkout location
> `D:\Projects\HerculesAC\...`.  The current repository lives at
> `D:\Project\Anticheat\herculesac-anticheat`, so those variables
> **must** be updated before the batch chain will find the .vcxproj
> files or the compiled outputs.  See *Known issues*.

### Building from Visual Studio directly

`HerculesAC.sln` opens cleanly in VS 2022.  Right-click **Build
Solution**, then run `vmp.bat` and `calculateMD5.bat` from the repo
root to produce the packed binaries and `hac.dat`.  The driver
(`herculeskernel`) has to be built with the WDK-aware toolset — pick
**x64** and set the deploy target if you use the WDK's Test/Deploy
integration.

### Configurations

| Solution config | User-mode | Kernel-mode |
| --------------- | --------- | ----------- |
| Debug / x86     | Debug\|Win32 for user-mode projects | forced to Debug\|x64 |
| Debug / x64     | Debug\|x64 for user-mode projects   | Debug\|x64 |
| Debug / ARM64   | mapped to Debug\|x64 for user-mode  | Debug\|ARM64 |
| Release / *    | mirrors the Debug row | mirrors the Debug row |

## Deployment layout

After a successful build (Debug used as the example) the runtime
directory should look like this:

```
<install dir>/
├── GameLauncher.exe
├── hac.dat
├── GameMon.aes           (VMProtect-packed GameMon.exe   x86)
├── GameMon64.aes         (VMProtect-packed GameMon.exe   x64)
├── GameProtect.dll       (x86)
├── GameProtect64.dll     (x64)
├── HerculesAC/
│   └── HerculesAC.aes    (VMProtect-packed HerculesAC.exe x64)
├── assets/
│   └── hac.bmp
├── hercules_log.txt      (created at runtime by Logger)
└── <game dir>/           (contains starter/Client from hac.dat, e.g. cstrike.exe)
```

The kernel driver (`herculeskernel.sys`) ships separately and is
installed the usual way — `sc create` + `sc start` on a test-signed
VM, or through the WDK deployment flow.

## Known issues / TODO

Ranked roughly by severity.  Everything here is a real observation
from the current tree — the code is functional research-grade and
does not pretend otherwise.

1. **Hard-coded protected PID `17608`** in
   `herculeskernel/Protect/Callback/Callbacks.cpp` (`preThreadCallback`
   and `preProcessCallback`).  The driver only ever protects that one
   PID; a real integration should learn the game PID via
   `IOCTL_START_PROTECT` (already defined in `Common/Shared/IOCTLs.h`
   but unused).
2. **Absolute paths in the batch files.**  `Project.bat`,
   `vmp.bat` (mostly relative now, but still names `D:\Projects\…`
   for the exe outputs) and `calculateMD5.bat` all bake in
   `D:\Projects\HerculesAC\...`.  These paths do not exist in the
   current checkout and will silently produce empty commands.  Convert
   them to `%~dp0`-relative before shipping.
3. **`wcscpy` with fixed 256-wchar buffers** for `szExe`, `sPath`
   and `sCommandLine` in both `HerculesAC` and `GameMon` `WinMain.cpp`.
   The JSON payload from `HerculesAC::InitProcess` is written straight
   into `sCommandLine[256]` — a long game directory will overflow.
   Use `wcscpy_s` + a larger buffer or `std::wstring` throughout.
4. **`wideStringToString` uses `CP_ACP` while `stringToWideString`
   uses `CP_UTF8`.**  A round-trip through both calls silently
   corrupts any non-ASCII path.  Pick one code page and be consistent
   (UTF-8 is the sensible default).
5. **`GameMon` accepts `hProcess` as `unsigned int` via JSON.**
   `HANDLE`s are process-local; passing them through JSON is
   meaningless in the child process and works only accidentally when
   the two processes happen to share a numeric value.  If a handle
   really needs to cross the process boundary use
   `DuplicateHandle(…, DUPLICATE_SAME_ACCESS, …)` + inheritable
   handles or IPC.
6. **CRC32 integrity checks cover only 5 bytes.**
   `InitCheckSum`/`CheckHookCode` snapshot the first 5 bytes of each
   monitored API — enough to catch a naïve inline patch, useless
   against IAT hooks, VEH-based hooks, or trampolines placed a few
   bytes deeper.
7. **`std::localtime` in `Logger::Log`.**  Not thread-safe on the
   Windows CRT; wrap in a local mutex (the mutex is already held for
   file I/O, so it is enough to switch to `localtime_s`).
8. **`__debugbreak()` after `RtlExitUserThread(0)`** in
   `NewBaseThreadInitThunk` — `RtlExitUserThread` does not return, so
   the trap is dead code.  Either remove it or replace the whole
   sequence with `TerminateThread`.
9. **Modal `MessageBox` on suspected thread starts** in
   `NewBaseThreadInitThunk` — locks the caller's UI while waiting for
   the user to click OK.  For a shipped anti-cheat this is a very
   noisy default; downgrade to a log line + silent kill.
10. **`Sleep(-1)` and hard-coded module offsets `0x37645` /
    `0x37510`** in `Bypass/HookCallSet/functionSet.cpp` are tied to a
    specific build layout of the target and will break as soon as the
    binary is rebuilt.  Treat the Bypass project as experimental only.
11. **`hercules_log.txt` sits next to the launcher** with no rotation
    and no size cap — the two 100 ms scanner loops can produce a lot
    of `outDebug` output over time.
12. **`certutil -hashfile` in `calculateMD5.bat`** returns the hash
    plus surrounding text; the current filter (`findstr /v "hash of
    file"`) drops the wrong line on non-English locales.  Consider
    switching to PowerShell's `Get-FileHash`.

## Recent history

Reading `git log --oneline`:

```
df90313 Update vmprotect path to relative directory
555e221 feat: check hacktools version              (VERSIONINFO string hashing)
369d319 feat: update sig                           (icon MD5 tables refreshed)
d134769 feat: detect hacktools by hash             (initial icon/bitmap hashing)
e59eb6f feat: support multiple games               (hac.dat-driven starter/Client)
d667c08 feat: update banner
cb44bf6 feat: add thread and process protect       (kernel Ob callbacks)
401d107 / dc1eafb feat: setup kernel mode driver
22835dd feat: add extra winapi signature checksum  (CRC32 integrity)
f369869 feat: add prevent thread creation for self-defense
f5e9b71 feat: add handle detection
0e49c1b / f84db7d / aca93fc feat: add automate script  (Builder / vmp / MD5 chain)
```

The most active detection surface is user-mode signature matching
(icon + version-info); the kernel driver is still in a “PoC with
hard-coded PID” state.
