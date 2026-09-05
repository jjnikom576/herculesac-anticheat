# Hercules Anti-Cheat

A modular Windows anti-cheat framework built for older MMO / FPS clients
(the packaged build targets `cstrike.exe`).  The project pairs a user-mode
watchdog + injected hook DLL with an optional kernel-mode handle-stripping
driver, all bound together with VMProtect virtualization and a signed
JSON manifest (`hac.manifest` + `hac.manifest.sig`) that ties the shipped
binaries together.

> **Disclaimer.**  This repository is intended for research and educational
> use — reverse-engineering, kernel programming, hook development and
> anti-cheat design.  It is **not** a production-ready product, ships with
> hard-coded debug values (see *Known issues* below), and must not be
> deployed on any system you do not own.  A number of techniques used
> here (global message hooks, kernel `Ob*` callbacks, VMProtect) are
> heavy-handed and will trip real EDR/AV products.

## Origin and attribution

This repository is a modified derivative of
[un4ckn0wl3z/HerculesAC](https://github.com/un4ckn0wl3z/HerculesAC), based on
upstream revision
[`df90313e483e49738bbf87f82bbdc148e1d32329`](https://github.com/un4ckn0wl3z/HerculesAC/commit/df90313e483e49738bbf87f82bbdc148e1d32329).
This derivative is published with permission: the original author granted
permission to modify and publicly redistribute it for educational purposes.
Copyright in the original source remains with the original author(s); this
repository does not assign or imply a standard open-source license for that
source.

Nikom Kawchoem (`jjnikom576`) is the maintainer of these modifications.
Verified major additions in this derivative include:

- signed manifest and module verification;
- dynamic kernel PID protection;
- targeted game-only injection;
- defensive detectors;
- telemetry and backend work; and
- CI, tests, and security hardening.

`Bypass` is an experimental validation fixture restricted to systems owned or
explicitly authorized by the researcher and isolated lab environments. It is
not a production component. This derivative does not imply endorsement by the
upstream author.

---

## Table of contents

1. [Origin and attribution](#origin-and-attribution)
2. [Component overview](#component-overview)
3. [Runtime architecture](#runtime-architecture)
4. [Detection techniques](#detection-techniques)
5. [Configuration file — `hac.manifest`](#configuration-file--hacmanifest)
6. [Repository layout](#repository-layout)
7. [Third-party code](#third-party-code)
8. [Building](#building)
9. [Deployment layout](#deployment-layout)
10. [Known issues / TODO](#known-issues--todo)
11. [Recent history](#recent-history)

---

## Component overview

The Visual Studio solution (`HerculesAC.sln`, VS 2022, platform toolset
v143) contains seven projects grouped into a **user-mode** folder and a
**kernel-mode** folder.

| Project          | Kind         | Arch     | Role |
| ---------------- | ------------ | -------- | ---- |
| `GameLauncher`   | `.exe`       | x64      | Thin bootstrapper the player double-clicks. Locates its own directory and spawns `HerculesAC\HerculesAC.aes` (post-VMProtect binary), passing the game path on the command line. |
| `HerculesAC`     | `.exe`       | x64      | The watchdog.  Verifies the signed `hac.manifest` (Ed25519 signature + per-module SHA-256), launches the game client, spawns both `GameMon` bitness variants, self-hooks `BaseThreadInitThunk` + `CreateThread` for self-defense, scans the process list / windows / resource icons / version strings for known hack tools, and CRC32-verifies its own hooks. |
| `GameMon`        | `.exe`       | x86 + x64 | Per-bitness monitor.  Loads `GameProtect(64).dll`, opens shared memory and publishes the game PID / paths for the injected DLLs to consume. |
| `GameProtect`    | `.dll`       | x86 + x64 | Targeted hook DLL.  `HerculesAC` injects it **only** into the game process via `CreateRemoteThread + LoadLibraryW`.  In `DllMain` it opens shared memory, then Detours-hooks `ReadProcessMemory` / `WriteProcessMemory`.  Any call whose target PID equals the protected game PID triggers termination logic.  (M3: global `WH_GETMESSAGE` hook removed.) |
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
   │  cstrike.exe ◄───┼────┤  HerculesAC      │      │  GameMon64.aes   │
   │  (game client)   │    │  CreateRemoteThread│     │  (x64 monitor)   │
   │                  │    │  + LoadLibraryW   │      └────────┬─────────┘
   │  GameProtect64   │    └──────────────────┘               │ LoadLibrary
   │    .dll loaded   │                                        ▼
   │    (M3 targeted) │                              ┌──────────────────┐
   └──────────────────┘                              │ GameProtect64.dll│
                                                     │  (x64 monitor    │
                                                     │   copy, no hook) │
                                                     └──────────────────┘

   ┌────────────────────────────────────────────┐
   │  DllMain in game process only (M3):        │
   │   • read hac.manifest + verify signature   │
   │   • open named shared memory (game PID)    │
   │   • Detours-hook ReadProcessMemory /       │
   │     WriteProcessMemory                     │
   │   • if caller targets the game PID and     │
   │     exceeds a rate threshold → terminate   │
   │     the game (deny-by-suicide)             │
   └────────────────────────────────────────────┘

           (optional, ring-0)
   ┌────────────────────────────────────────────┐
   │  herculeskernel.sys (altitude 321000)      │
   │  • Ob* callbacks strip VM_READ/WRITE and   │
   │    SUSPEND_RESUME from handles to game PID │
   │  • PsSetCreateThreadNotifyRoutine: logs    │
   │    new threads created in protected PIDs   │
   └────────────────────────────────────────────┘
```

### Launch sequence (annotated)

1. **`GameLauncher::WinMain`** — determines its own directory and calls
   `StartProcess(dir, L"HerculesAC\\HerculesAC.aes", L" " + dir)`.  The
   `.aes` extension is the VMProtect-packed twin of `HerculesAC.exe`;
   the batch scripts run VMProtect on every shipping binary and rename
   the output.  (See `vmp.bat`.)
2. **`HerculesAC::WinMain`** — takes a singleton lock (`HACMutexName`
   mutex), shows the splash bitmap `assets\hac.bmp` for 5 s, then:
   - Loads and verifies `hac.manifest`: `Manifest::Load` parses the JSON,
     `Manifest::VerifySignature` checks the Ed25519 detached signature
     against the compile-time embedded public key, then
     `Manifest::VerifyModule` SHA-256-hashes every referenced binary and
     asserts it is present and unmodified. Any failure fails closed
     (event log + `MessageBox` + exit) before the game is ever launched.
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
3. **`HerculesAC::InitProcess` (M3 injection)** — after the game process
   is created, `hac::driver::StartProtect(pid)` registers it with the
   kernel driver via `IOCTL_START_PROTECT`, and `hac::inject::InjectDll`
   calls `CreateRemoteThread + LoadLibraryW` to load `GameProtect64.dll`
   (x64 build) directly into the game process.  No global hook; no other
   process is touched.
4. **`GameMon::WinMain`** — parses the JSON, singleton-locks
   (`GameMonMutexName` / `GameMonMutexName64`), loads
   `GameProtect(64).dll` for its own reference, opens the shared memory
   (`hacSharedMemory` / `hacSharedMemory64`), writes the current game PID /
   client name / game directory into it.  `SetupHook` / `UnHook` are no-ops
   (global `WH_GETMESSAGE` hook removed in M3).
5. **`GameProtect::DllMain`** — runs *only* in the game process (M3):
   reads and verifies `hac.manifest` (Ed25519 signature), installs Detours
   hooks for `ReadProcessMemory` and `WriteProcessMemory`, and exports
   `GetSharedMemoryPtr`.  `IsWhitelistCurrentProcess` removed — targeted
   injection makes per-process whitelisting unnecessary.

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

## Configuration file — `hac.manifest`

A signed JSON (schema `version: 2`) file generated at build time by
`tools/gen-manifest.ps1` and read by every user-mode component through
`hac::manifest::Manifest` (`Common/Manifest/Manifest.h`). It replaces the
old `hac.dat` INI + MD5 table (see *Recent history*) with a cryptographically
verified equivalent.

```json
{
  "version": 2,
  "issued_at": "2026-09-15T00:00:00Z",
  "game": {
    "id": "cstrike-1.6",
    "starter": "cstrike.exe",
    "client": "cstrike.exe",
    "monitor_x86": "GameMon.aes",
    "monitor_x64": "GameMon64.aes"
  },
  "modules": [
    { "path": "HerculesAC\\HerculesAC.aes", "sha256": "<sha256 hex>", "size": 123456 },
    { "path": "GameMon.aes",                "sha256": "<sha256 hex>", "size": 123456 },
    { "path": "GameMon64.aes",              "sha256": "<sha256 hex>", "size": 123456 },
    { "path": "GameProtect.dll",            "sha256": "<sha256 hex>", "size": 123456 },
    { "path": "GameProtect64.dll",          "sha256": "<sha256 hex>", "size": 123456 }
  ],
  "whitelist_sha256": ["<sha256 hex>", "..."],
  "reporting": {
    "endpoint": "https://ac-report.example.com/v1/events",
    "server_cert_pin_sha256": ""
  }
}
```

- **`game`** — which executable is the game (`starter`/`client`) and which
  packed monitor binaries (`monitor_x86`/`monitor_x64`) HerculesAC spawns.
  Replaces the old `[Game]` INI section.
- **`modules`** — every shipped binary's relative path, SHA-256 hash and
  byte size. `Manifest::VerifyModule` re-hashes the file on disk and
  rejects a mismatch (`ModuleHashMismatch`) or a missing file
  (`ModuleMissing`). Replaces the old `[MD5]`/`Count` INI section.
- **`whitelist_sha256`** — the SHA-256 hashes `GameProtect(64).dll`
  compares its own host-module hash against before deciding whether to
  install the RPM/WPM hooks in a given process. This is how the
  watchdog / monitor / launcher / game itself avoid hooking themselves.
- **`reporting`** — the telemetry endpoint and (optional) certificate pin
  used by future reporting features.

**Signature.** `hac.manifest` ships next to a sibling `hac.manifest.sig` —
a raw 64-byte Ed25519 detached signature (libsodium) over the manifest's
exact bytes, produced by `tools/sign-manifest` from the private key at
`tools/keys/hac-dev.private.key`. Every consumer (`HerculesAC`, `GameMon`,
`GameProtect`) calls `Manifest::Load` then `Manifest::VerifySignature`
against a public key **embedded at compile time**
(`Common/Manifest/PublicKey.h`, `HAC_PUBKEY_HEX`, default
`046070fafe0c77fb20be806702cd95e76b1e6074cb1da2c75aacd990c21fc456` — the
checked-in dev key at `tools/keys/dev.public.key.hex`) — the public key is
never read from the deploy directory, so a tampered manifest cannot supply
its own matching key.

**Fail-closed.** Any `ManifestError` (missing file, oversized file,
malformed JSON, unsupported `version`, invalid signature, missing/mismatched
module) stops the boot sequence before the game is launched. `HerculesAC`
mirrors the failure to the Windows Event Log (`Application/HerculesAC`,
event ID `1001`) via `WriteEventLog`, then shows a `MessageBox` and exits —
see `docs/testing/m1-e2e-tamper.md` for the manual test procedure that
exercises every one of these paths.

Changing the target game means regenerating the manifest with
`tools/gen-manifest.ps1 -GameId <id> -GameDir <path> -PrivateKey
tools\keys\hac-dev.private.key` rather than hand-editing an INI file.

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
├── Builder.bat                   Full solution build (x86 + x64) → vmp → hac.manifest
├── ReBuilder.bat                 Same as above with /t:Rebuild
├── Project.bat                   Repo-relative paths to each .vcxproj + build config
├── vmp.bat                       Runs VMProtect_Con.exe on the three shipped exes
│
├── tools/
│   ├── gen-manifest.ps1          Emits hac.manifest + hac.manifest.sig at build time
│   ├── sign-manifest/            Ed25519 detached-signature CLI (libsodium)
│   ├── keys/                     Dev signing keypair (hac-dev.private.key + dev.public.key.hex)
│   └── register-eventlog.ps1     One-time Application/HerculesAC event source registration
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
- **PowerShell 5.1+** (in-box on Windows) is used by
  `tools/gen-manifest.ps1` (via `Get-FileHash`) to compute module SHA-256
  hashes and emit `hac.manifest` / `hac.manifest.sig`.
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
tools\gen-manifest.ps1 -> emits x64\Debug\hac.manifest + hac.manifest.sig
                          (SHA-256 module table, signed with tools\keys\hac-dev.private.key)
```

`ReBuilder.bat` does the same with `/t:Rebuild`.  Both call
`pause` at the end so the console does not close on error.

### Building from Visual Studio directly

`HerculesAC.sln` opens cleanly in VS 2022.  Right-click **Build
Solution**, then run `vmp.bat` and `tools\gen-manifest.ps1` from the repo
root to produce the packed binaries and `hac.manifest` / `hac.manifest.sig`.
The driver (`herculeskernel`) has to be built with the WDK-aware toolset —
pick **x64** and set the deploy target if you use the WDK's Test/Deploy
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
├── hac.manifest
├── hac.manifest.sig
├── GameMon.aes           (VMProtect-packed GameMon.exe   x86)
├── GameMon64.aes         (VMProtect-packed GameMon.exe   x64)
├── GameProtect.dll       (x86)
├── GameProtect64.dll     (x64)
├── HerculesAC/
│   └── HerculesAC.aes    (VMProtect-packed HerculesAC.exe x64)
├── assets/
│   └── hac.bmp
├── hercules_log.txt      (created at runtime by Logger)
└── <game dir>/           (contains starter/Client from hac.manifest, e.g. cstrike.exe)
```

The kernel driver (`herculeskernel.sys`) ships separately and is
installed the usual way — `sc create` + `sc start` on a test-signed
VM, or through the WDK deployment flow.

## Known issues / TODO

Ranked roughly by severity.  Everything here is a real observation
from the current tree — the code is functional research-grade and
does not pretend otherwise.

1. **Absolute paths in the batch files.**  `Project.bat` and the
   manifest generation step are now repo-relative, but `vmp.bat`'s
   `VMProtect_Con.exe` path still points outside the checkout
   (`..\anticheat\VMProtect Ultimate v3.3.1 …`).  Convert it to a
   `%~dp0`-relative or environment-variable-driven path before shipping.
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
12. **The dev private key at `tools/keys/hac-dev.private.key` is a
    shared credential** — anyone with commit access can produce a
    valid manifest. Rotate before pilot deployments (see
    `docs/signing.md` — coming in M6).

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

**M2 Kernel Authority (2026-09-03):** The driver now manages a dynamic PID
table via a `\Device\HerculesAC` IOCTL surface.  `IOCTL_START_PROTECT`
(requires SeDebugPrivilege) inserts a PID; `IOCTL_STOP_PROTECT` removes it;
`IOCTL_QUERY_STATUS` returns the current count and driver version
(`0x00020000`).  Ob callbacks consult the table at runtime — the hard-coded
PID 17608 has been removed.  `HerculesAC.exe::InitProcess` calls
`StartProtect(pid)` automatically after launching the game starter.
Build: `Builder.bat` now routes the kernel target through VS 2022 MSBuild
(required by WDK 26100).  See `docs/testing/m2-e2e-driver-load.md`.
