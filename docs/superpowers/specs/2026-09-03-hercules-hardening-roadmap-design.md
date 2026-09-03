# Hercules Anti-Cheat — Hardening Roadmap (design spec)

**Status:** draft
**Author:** jjnikom576 (with Claude Code)
**Date:** 2026-09-03
**Session:** https://claude.ai/code/session_011gJWcQS8FUmGu9NKctEud4

---

## 1. Context

`hercules-anticheat` (currently checked out at `D:\Project\Anticheat\herculesac-anticheat`) is a modular Windows anti-cheat that pairs a user-mode watchdog + injected hook DLL with a kernel-mode handle-stripping driver. The main components are:

- `HerculesAC.exe` — watchdog / orchestrator (x64)
- `GameLauncher.exe` — thin bootstrapper (x64)
- `GameMon.exe` / `GameMon64.exe` — per-bitness monitors
- `GameProtect.dll` / `GameProtect64.dll` — the injected hook DLL
- `herculeskernel.sys` — ring-0 `Ob*` callbacks driver
- `Common/` — shared utility code (Detours, IPC, hashing, VMProtect SDK)
- `Bypass/`, `ExampleDLL/` — research helpers, out of scope for this roadmap

A code review of the current tree (see `README.md`) surfaced twelve concrete correctness / quality issues plus five categories of deeper architectural concerns. Those concerns motivate this roadmap.

## 2. Goals

1. **Trust chain is verifiable end-to-end.** Every config file, every module the anti-cheat loads, and every PID the driver protects must be provably intact; an attacker who edits any of them with a hex editor must be detected before the game process is even created.
2. **The kernel driver is the authority on what is protected.** The user-mode watchdog does not decide protection state on its own — it *requests* protection over an IOCTL, and the kernel verifies the request. The hard-coded PID `17608` is removed.
3. **The user-mode injection surface stops being hostile to the host OS.** The `WH_GETMESSAGE` global message hook that broadcasts `GameProtect.dll` into every UI process on the desktop is removed; `GameProtect.dll` is injected into the protected game process *only*.
4. **A detection-first workflow is real.** Detectors emit typed events over a documented wire protocol into an offline queue that survives disconnects and delivers to a backend when the network comes back. The backend itself is out of scope, but the client-side contract is complete.
5. **The project is shippable to a production public server.** Signed builds, MSI installer, CI, static analysis, log rotation, and an EV-cert + WHQL path are in place. Test-signing is used only during development.

## 3. Non-goals

The following are *deliberately* excluded from this roadmap so it stays deliverable:

- **Backend server implementation.** This roadmap defines the wire protocol and the client-side reporter; the receiving service, admin dashboard, ban tooling, and database are separate work owned by the backend team.
- **Multi-game orchestration.** The manifest schema is multi-game-*ready* (a `game.id` field is present), but the runtime still assumes one HerculesAC instance protects one game process.
- **Hypervisor-level cheat detection** (DBVM, EFI-level, Ring-2). Noted as post-roadmap future work.
- **Cross-platform.** Windows 10 and Windows 11 only.
- **Hardening of `Bypass/` or `ExampleDLL/`.** They stay in the tree as research playgrounds.
- **Replacing VMProtect.** VMProtect is still the packer of record; only its invocation path is fixed to be repo-relative.

## 4. Milestone map

The roadmap is organised **foundation-first**: each milestone strengthens a layer that the next one relies on. Executing them out of order would waste work — detection built on top of an untrusted config, for example, is worthless because the config itself can be edited to whitelist cheats.

| # | Name | Entry criteria | Exit criteria |
| - | ---- | -------------- | ------------- |
| M1 | Trusted config v2 | this spec approved | signed `hac.manifest` loads on all five shipping modules; a tampered manifest or module refuses to start; migration script replaces `calculateMD5.bat`. |
| M2 | Kernel authority | M1 complete | `IOCTL_START_PROTECT` accepts a `{pid, image_name, sha256}` request, verifies caller identity, and adds the PID to a runtime protection table. The hard-coded `17608` is gone. Section-object–based memory access is blocked by a minifilter. |
| M3 | Usermode injection redesign | M2 complete | `WH_GETMESSAGE` global hook is removed. `GameProtect.dll` appears in the module list of the game process only. Microsoft Defender and CrowdStrike Falcon on a stock install raise no alerts on the watchdog / monitor / DLL trio. |
| M4 | Reporting protocol | M1 complete (can run in parallel with M2–M3) | Protobuf schema `AntiCheatEvent` is frozen and versioned. Client emits events into an offline SQLite queue and delivers them over mTLS to a configurable endpoint. `docs/backend-contract.md` is complete and reviewed. |
| M5 | Detection expansion | M3 and M4 complete | A detector-plugin architecture is in place. IAT/EAT hook scanning, extended thread-injection detection, code-section integrity scans, and driver-enumeration checks are implemented. A scripted red-team suite passes with detection latency < 5 seconds per case. |
| M6 | Ops + release | M5 complete | GitHub Actions CI builds Debug/Release × x86/x64 on every push; a git tag `v*` produces a signed MSI installer end-to-end. Logs rotate daily with 7-file retention. Repo paths are fixed to `%~dp0`-relative and `assets/hacktools-sig/*.bin` moves to Git LFS. |

Rough timeline for a single maintainer: M1 + M2 ≈ 2 sprints, M3 = 1 sprint, M4 = 1 sprint, M5 = 2 sprints, M6 = 1 sprint. Total ≈ 2–3 months.

## 5. Milestone details

### 5.1 M1 — Trusted config v2

**Motivation.** `hac.dat` today is a plaintext INI with MD5 hashes. An attacker who owns the machine can trivially rewrite the `[MD5]` section to whitelist a cheat DLL. MD5 has been considered cryptographically broken since 2004; keeping it for integrity is indefensible on a production build.

**Design.**

- Replace `hac.dat` with two files in the install root: `hac.manifest` (canonical JSON) and `hac.manifest.sig` (raw Ed25519 detached signature over the manifest bytes).
- Manifest schema, version 2:
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
      { "path": "HerculesAC/HerculesAC.aes", "sha256": "…", "size": 1234567 },
      { "path": "GameMon.aes",               "sha256": "…", "size": … },
      { "path": "GameMon64.aes",             "sha256": "…", "size": … },
      { "path": "GameProtect.dll",           "sha256": "…", "size": … },
      { "path": "GameProtect64.dll",         "sha256": "…", "size": … }
    ],
    "whitelist_sha256": [
      "…",
      "…"
    ],
    "reporting": {
      "endpoint": "https://ac-report.example.com/v1/events",
      "server_cert_pin_sha256": "…"
    }
  }
  ```
- **Signing.** Ed25519 via libsodium. The private key never leaves the build environment (Azure Key Vault + `sigstore` alternatives are noted as future work). The public key is embedded in `HerculesAC.exe` at compile time as a `const uint8_t[32]`.
- **Verification flow at boot.** HerculesAC memory-maps `hac.manifest` + `hac.manifest.sig`, calls `crypto_sign_verify_detached`, and — on success — computes SHA-256 of every module referenced in `modules[]` and compares against the manifest. Any mismatch → refuse to launch, write a critical event to the Windows Event Log (source `HerculesAC`), and exit.
- **New library.** `Common/Manifest/` exposes `Manifest::Load(path)`, `Manifest::VerifySignature(pubkey)`, `Manifest::VerifyModule(module_path)`, and `Manifest::GameConfig()` for consumers.
- **Migration.** A PowerShell script `tools/gen-manifest.ps1` replaces `calculateMD5.bat`. It walks the build output, computes SHA-256 via `Get-FileHash`, calls a small signing helper (`tools/sign-manifest.exe`) with the private key, and emits both `hac.manifest` and `hac.manifest.sig`. `calculateMD5.bat` is deleted, not kept as a fallback.

**Error handling.** Fail closed. Every verification step returns a typed error (`ManifestError::SignatureInvalid`, `ManifestError::ModuleMissing`, `ManifestError::ModuleHashMismatch`); the boot path treats *any* of them as a hard failure and refuses to continue. Errors go to the Event Log; `hercules_log.txt` is no longer trusted for security-critical output because user-mode processes can write to it.

**Test surface.** `Common/Manifest/tests/` gets gtest cases for: valid manifest, tampered payload, wrong signature, missing module, wrong-sized module, wrong-hashed module, malformed JSON, oversized manifest.

### 5.2 M2 — Kernel authority

**Motivation.** `herculeskernel/Protect/Callback/Callbacks.cpp` currently protects PID `17608` and only PID `17608`. There is no path for the user-mode watchdog to tell the driver which PID is the real game.

**Design.**

- Driver creates a control device at `\Device\HerculesAC` with symlink `\\.\HerculesAC` (the symbolic-link name is already defined in `Common/Shared/SharedStruct.h`).
- IOCTL surface, defined in `Common/Shared/IOCTLs.h`:

  | IOCTL | Direction | Payload |
  | ----- | --------- | ------- |
  | `IOCTL_START_PROTECT` | IN | `{ DWORD pid; WCHAR image_name[260]; BYTE expected_sha256[32]; }` |
  | `IOCTL_STOP_PROTECT`  | IN | `{ DWORD pid; }` |
  | `IOCTL_QUERY_STATUS`  | OUT | `{ DWORD active_protection_count; DWORD driver_version; }` |
  | `IOCTL_CHECK_VT` | (already reserved) | future |
  | `IOCTL_CHECK_SYMBOLICLINK` | (already reserved) | future |

- **Protected-PID table.** Backed by `RTL_GENERIC_TABLE`, guarded by a `FAST_MUTEX`. Insert / remove / lookup are all O(log n). The `Ob*` pre-op callbacks read from this table instead of comparing against a constant.
- **Caller authentication.** The IOCTL handler resolves the caller's `PEPROCESS`, computes SHA-256 of its main image via a section-object read (never a user-mode file open — user mode could swap the file), and refuses any IOCTL whose caller image hash is not on a compile-time list embedded in the driver. That list contains only the HerculesAC image hash. In effect, a cheat cannot call `IOCTL_START_PROTECT` itself to add a rogue PID.
- **Verification before insertion.** When HerculesAC requests protection for a PID, the driver:
  1. Opens the target's `PEPROCESS` via `PsLookupProcessByProcessId`.
  2. Reads its main image name from `_EPROCESS::ImageFileName` (via SymbolicAccess, already vendored).
  3. Section-reads the target image and computes SHA-256.
  4. Requires both to match the manifest values before adding to the table.
- **Callback expansion.**
  - Keep the existing `PsProcessType` / `PsThreadType` `ObRegisterCallbacks` at altitude `321000`.
  - Add `PsSetCreateProcessNotifyRoutineEx` to observe child creation. Newly spawned children of a protected PID are auto-added if their image hash matches an allowlist; unknown children get logged (M4 events) and, if severity is high, force-terminated.
  - Add a **minifilter** (`FltRegisterFilter`) targeting `IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION` on the game image. This blocks section-object–based memory reads that today's `Ob*` callbacks cannot catch (a cheat that opens a section via `NtCreateSection` from the game file and maps it in its own process would bypass the `PROCESS_VM_READ` check entirely).
- **Failure mode.** If the driver refuses the IOCTL for any reason, HerculesAC treats it as fatal: log critical event, kill the launched game process, exit. There is no soft-fail path.
- **New files.** `herculeskernel/Ioctl/{Dispatch,Handler}.{h,cpp}`, `herculeskernel/Protect/PidTable.{h,cpp}`, `herculeskernel/Protect/MiniFilter/{FilterMain,SectionFilter}.{h,cpp}`.

**Error handling.** IOCTLs return typed `NTSTATUS` values (`STATUS_ACCESS_DENIED` for caller-auth failure, `STATUS_OBJECT_NAME_INVALID` for hash mismatch, `STATUS_INVALID_PARAMETER` for malformed buffers). All are logged with `DbgPrintEx` to `DPFLTR_IHVDRIVER_ID` and duplicated as WPP traces so ETW can pick them up in production.

**Test surface.** HLK-style test driver + a Windows 10 and Windows 11 VM snapshot. Positive test: HerculesAC-signed image can add / remove PIDs. Negative tests: cheat image cannot add PIDs; wrong-hash target PID is refused; section-object cheat is blocked by the minifilter; concurrent add / remove from many threads does not race (stress-test with 32 threads × 1000 iterations).

### 5.3 M3 — Usermode injection redesign

**Motivation.** `SetWindowsHookEx(WH_GETMESSAGE, …)` is called from `GameMon` today, which causes Windows to load `GameProtect.dll` into every UI process on the desktop. On a modern Windows install this is flagged by Defender's ASR "block credential stealing from LSASS" and by CrowdStrike as suspicious injection; even when it does not outright block, it fills the user's system-wide logs with our DLL. It is also a privacy problem — our code runs inside the user's browser, office suite, and password manager.

**Design.**

- Delete `SetupMsgHook` / `UnHookMsgHook` / `GetMsgProc` from `GameProtect/Hook/MsgHook/` and the callers in `GameMon`.
- Injection is triggered from the kernel driver. When `IOCTL_START_PROTECT` succeeds (M2), the driver registers a `PsSetCreateProcessNotifyRoutineEx` hook. On the next process-create notification whose PID matches the protected PID, the driver:
  1. Waits (using a work item queued to a system worker thread) for the user-mode address space to be ready.
  2. Queues a user-mode APC via `KeInitializeApc` + `KeInsertQueueApc`. The APC routine is the entrypoint of a small stub that calls `LdrLoadDll` on `GameProtect(64).dll` from the install directory.
- **Two implementation choices for the load itself** — decided at M3 detailed-plan time:
  - *Simpler path (recommended default):* the APC calls the standard `LdrLoadDll` on the DLL. Fast to implement, module shows up in `EnumProcessModules` (which is fine because it is a legitimately signed module).
  - *Stealth path (optional):* the APC calls a manual-mapper routine embedded in the stub, avoiding `LdrLoadDll` entirely. `GameProtect.dll` no longer appears in the loaded-module list, which raises the bar for cheats that enumerate modules. Costs are: no automatic TLS initialisation, higher implementation complexity, and no DllMain-based cleanup.

  The recommendation is the simpler path for the first release, with the stealth path deferred to a future hardening pass.
- **Per-process IPC.** The existing named `SharedMemory` (`hacSharedMemory` / `hacSharedMemory64`) stays, but now serves only one host process (the game itself) plus GameMon. `GetSharedMemoryPtr` remains the export used by GameMon to publish `GAME_CLIENT` into it.
- **Whitelisting is no longer a runtime concern.** With targeted injection, `GameProtect.dll` never enters HerculesAC, GameMon, or the launcher. The `IsWhitelistCurrentProcess` check inside `GameProtect::DllMain` (which today reads `[MD5]` out of `hac.dat`) is removed.

**Error handling.** APC insertion can fail if the target process is exiting. On failure the driver logs a critical event and force-terminates the target, on the assumption that a game process that cannot receive our protection is not safe to run.

**Test surface.** Boot a stock Windows 10 22H2 and Windows 11 24H2 VM with Defender fully enabled, run through the full launcher → game flow, and confirm Defender's timeline contains zero HerculesAC alerts. Repeat with CrowdStrike Falcon and Sysmon (using the SwiftOnSecurity ruleset) to catch regressions.

### 5.4 M4 — Reporting protocol and evidence collection

**Motivation.** Detection-first was the chosen anti-cheat philosophy. That requires typed evidence, delivered reliably, that a backend team can consume to run human review and ban workflows. Today the anti-cheat only writes free-form strings to `hercules_log.txt`, which is unusable for automated review.

**Design.**

- **Wire format.** Protocol Buffers v3, one message per event. Schema lives in `docs/proto/anticheat.proto`, versioned via the file's own `syntax = "proto3"; package hac.v1;`. Adding fields is always additive; removing fields requires a new `hac.v2` package and a client-side version bump.
  ```proto
  syntax = "proto3";
  package hac.v1;

  message AntiCheatEvent {
    string client_id            = 1;   // per-machine, HMAC(hardware_id, install_secret)
    string session_id           = 2;   // per-game-launch UUIDv4
    uint64 timestamp_utc_ms     = 3;
    string game_id              = 4;   // matches manifest.game.id
    Severity severity           = 5;
    DetectionKind kind          = 6;
    string detector_version     = 7;   // "iat-hook@1.2.0"
    bytes  evidence_bundle      = 8;   // opaque, DetectionKind-specific
    string signature            = 9;   // HMAC-SHA256 over fields 1..8, session-scoped key
  }

  enum Severity      { UNKNOWN = 0; INFO = 1; WARN = 2; CRITICAL = 3; }
  enum DetectionKind {
    KIND_UNKNOWN         = 0;
    PROC_NAME            = 1;
    WIN_NAME             = 2;
    MODULE_HASH          = 3;
    RPM_RATE             = 4;
    WPM_TARGET           = 5;
    HOOK_INTEGRITY       = 6;
    IAT_HOOK             = 7;
    EAT_HOOK             = 8;
    CODE_SECTION_DIVERGE = 9;
    THREAD_INJECT        = 10;
    KNOWN_BAD_DRIVER     = 11;
  }
  ```
- **Evidence bundle per kind.** Each kind is a nested protobuf message serialised into `evidence_bundle`. Examples:
  - `PROC_NAME`: `{ string image_name; uint32 pid; uint32 parent_pid; string image_sha256; }`
  - `RPM_RATE`: `{ string caller_image_sha256; uint32 caller_pid; double rate_per_second; uint32 duration_ms; repeated uint64 stack_top_8_frames; }`
  - `HOOK_INTEGRITY`: `{ string api_name; bytes expected_prologue; bytes observed_prologue; string hooking_module_sha256; uint64 hook_target_offset; }`
- **Transport.** HTTPS + mTLS. The endpoint URL, the pinned server-cert SHA-256, and the client cert path all come from the signed manifest (`reporting.*`). No plaintext HTTP fallback.
- **Offline queue.** SQLite database at `%LOCALAPPDATA%\HerculesAC\queue.db`, circular with a 10 MB cap. Events are enqueued synchronously (single writer thread), delivered asynchronously by a background sender thread that retries with exponential backoff (1s, 5s, 30s, 5m, 30m, cap 30m). Successful delivery removes the row.
- **New library.** `Common/Reporting/` — `Reporter::Emit(AntiCheatEvent)`, `Reporter::Flush()` (used at shutdown), `Reporter::SetEndpoint(manifest)`. Only one instance per process; owned by HerculesAC and GameMon; GameProtect posts events via the shared-memory channel and lets GameMon forward them (GameProtect must not open outbound sockets from inside the game process, for AV and firewall reasons).
- **Backend contract document.** `docs/backend-contract.md` — endpoint URL layout, mTLS certificate rotation flow, HTTP status semantics (2xx = accepted, 4xx = drop-do-not-retry, 5xx = retry, 429 = back off), rate-limit expectations (client caps itself at 10 events/s with burst 50), and the JSON schema for the `bytes evidence_bundle` decoded per `DetectionKind`.

**Error handling.** All send errors are retryable except the 4xx class. Malformed events are dropped and logged locally. A full queue evicts the oldest row and increments a local counter, which is surfaced as an `INFO` `SelfDiagnostic` event on the next successful send.

**Test surface.** Unit tests for the queue (fill, evict, restart-persistence, corruption recovery). A mock server (Python Flask + `pytest-httpserver`) in `tools/mock-server/` that a developer runs locally to iterate on the reporter without needing the real backend; the mock server logs everything and can be scripted to simulate 4xx / 5xx / 429 / slow responses.

### 5.5 M5 — Detection expansion

**Motivation.** With the trust chain in place (M1–M4), it is now safe to invest in detection. Doing this before the foundation would waste effort — every check added would be trivially defeated by editing `hac.dat` or by injecting into HerculesAC itself through the global hook path.

**Design.**

- **Detector plugin architecture.** `HerculesAC/Detectors/IDetector.h` defines:
  ```cpp
  class IDetector {
  public:
    virtual ~IDetector() = default;
    virtual std::string_view Name()    const noexcept = 0;
    virtual DetectionKind    Kind()    const noexcept = 0;
    virtual std::chrono::milliseconds Interval() const noexcept = 0;
    virtual void Poll(Reporter& out) = 0;
  };
  ```
  A single scheduler thread walks a registry of `std::unique_ptr<IDetector>` and calls `Poll()` at each detector's declared interval; each detector emits zero or more events via the injected `Reporter`.
- **Detectors delivered by M5:**

  | Detector | Kind | Interval | Notes |
  | -------- | ---- | -------- | ----- |
  | `ProcessSigDetector`   | `MODULE_HASH`          | 10s | Walks every running process, hashes the image with SHA-256, matches against a YARA rule set. Replaces the icon-MD5 approach. |
  | `IATHookDetector`      | `IAT_HOOK`             | 30s | Walks `IMAGE_IMPORT_DESCRIPTOR` for critical modules (`ntdll`, `kernel32`, `user32`, the game module), compares each thunk to its export in the loaded original. |
  | `EATHookDetector`      | `EAT_HOOK`             | 30s | Same idea against `IMAGE_EXPORT_DIRECTORY`. |
  | `PrologueDetector`     | `HOOK_INTEGRITY`       | 5s  | Disassembles the first 32 bytes of a curated API list (not 5 as today) and matches against the export in the on-disk module image. |
  | `CodeSectionDetector`  | `CODE_SECTION_DIVERGE` | 30s | SHA-256 of the game module's `.text` sections against the disk baseline. |
  | `ThreadInjectDetector` | `THREAD_INJECT`        | driven by kernel notifications | Consumes `PsSetLoadImageNotifyRoutine` and `PsSetCreateThreadNotifyRoutine` events forwarded from the driver via a shared ring buffer; flags loads whose backing file is not on the manifest allowlist. |
  | `DriverEnumDetector`   | `KNOWN_BAD_DRIVER`     | 60s | `NtQuerySystemInformation(SystemModuleInformation)`, matches loaded driver names / hashes against a blacklist (Cheat Engine driver `dbk64.sys`, `GH-Injector-*.sys`, VMware backdoor drivers, etc). |

- **Rule data.** YARA rules and known-bad-driver hashes live in `assets/rules/` and are covered by the manifest (they get their own `sha256` entries in `modules[]`).

**Error handling.** A detector that throws is caught in the scheduler, logged as a `SelfDiagnostic` event, and skipped for its next interval. Three consecutive failures deactivate the detector for the session and raise `WARN`.

**Test surface.** `tests/redteam/` contains a scripted cheat harness — a small process that (a) opens the game process with `PROCESS_VM_READ`, (b) reads memory in a loop at N Hz, (c) patches a known API prologue with `E9 xx xx xx xx`, (d) manual-maps a DLL, (e) creates a remote thread. Each test asserts the corresponding detector emits an event within 5 seconds. CI runs the harness inside a nested-VM job.

### 5.6 M6 — Ops and release engineering

**Motivation.** The current build story is `Builder.bat` in a developer's shell, VMProtect Ultimate on the machine that happens to have it, and `certutil` for hashing. That is fine for research but not for production.

**Design.**

- **CI.** GitHub Actions, three workflows:
  - `build.yml` — matrix (`{ Debug, Release } × { x86, x64 }`) using `windows-2022` runners, msbuild, artefact upload.
  - `analysis.yml` — clang-tidy on the user-mode C++ code, MSVC `/analyze` on the whole solution, PVS-Studio community license nightly. Warnings that are new since the previous main-branch commit fail the PR check.
  - `release.yml` — trigger on git tag `v*`, build Release/x86 + x64, run `tools/gen-manifest.ps1`, run `signtool` (test cert in dev, EV cert in prod), package MSI via WiX, upload artefacts to the GitHub Release.
- **Installer.** WiX Toolset. Install path `%ProgramFiles%\HerculesAC\`. On install: register the kernel driver as an auto-start service (`sc create HerculesACKM binPath= ... start= auto`), add a Windows Firewall exception for the reporting endpoint, write install info under `HKLM\SOFTWARE\HerculesAC`, drop a per-user shortcut. The uninstaller removes all of the above; this is the first uninstaller the project has ever had.
- **Logging.** `Common/Logger/` is replaced with a thin wrapper over **spdlog**. Rotation: daily, seven-file retention, per-severity file. Anything at `CRITICAL` is duplicated to the Windows Event Log via `ReportEvent`.
- **Repo hygiene:**
  - `Project.bat`, `calculateMD5.bat`, `vmp.bat`, `Builder.bat`, `ReBuilder.bat` all move to `%~dp0`-relative paths.
  - `calculateMD5.bat` is removed (replaced by `tools/gen-manifest.ps1` in M1).
  - `assets/hacktools-sig/*.bin` moves to Git LFS. History is rewritten in a single dedicated PR that documents the SHA changes so anyone rebasing knows what happened.
  - The mojibake comment at `HerculesAC/Threads/ActiveThread.cpp:270` is corrected.
  - Dead code is deleted: the commented `CheckHandleName`, the commented `Hook_02_Crc32` / `Hook_CreateToolhelp32Snapshot` in `Bypass`, and the commented-out critical section in `SharedMemory` (either restore it and use it, or delete it — the M4 architecture makes it unnecessary because we no longer share a single SHM across many processes).
- **Signing pipeline:**
  - **Dev.** Local dev cert, test-signing enabled on the developer VM.
  - **Staging.** Attestation signing through Microsoft Partner Center — no HLK, ships to internal testers.
  - **Prod.** EV code-signing cert (~USD 300/year) + WHQL submission. Full process is documented in `docs/signing.md`.

**Error handling.** CI failures block merges. Installer failures roll back via the MSI transaction. Log write failures degrade to the Event Log.

**Test surface.** CI runs the full unit + red-team suite on every push. Installer is smoke-tested manually per release on a clean Windows 10 22H2 and Windows 11 24H2 VM.

## 6. Cross-cutting concerns

### 6.1 Testing strategy

- **Unit** — gtest for every library under `Common/`. Each detector has an isolated test that injects a fake process list and asserts the events emitted.
- **Integration (user-mode)** — launch a dummy game process (a small stub that just `Sleep`s), attach GameProtect via M3's kernel-driven APC path, run scripted cheat behaviours from a second process, assert events on the reporter.
- **Integration (kernel)** — HLK-style test driver on a Windows 10 and Windows 11 VM snapshot. Snapshots are re-created weekly to catch OS-update regressions.
- **Red-team** — `tests/redteam/` (see M5). Latency budget: any detector must fire within 5 s of the offending behaviour starting.
- **CI gate** — every PR must be green across unit, integration, and red-team before merge.

### 6.2 Development workflow

- **Branching** — trunk-based, feature branches, PR requires green CI + one reviewer.
- **Cadence** — one milestone per 1–2 sprints. Every milestone ends with a tagged pre-release build so QA can smoke-test on real hardware.
- **Docs** — every milestone updates `docs/architecture.md` and appends to `CHANGELOG.md`.
- **Working with Claude Code** — every milestone gets its own `docs/superpowers/specs/YYYY-MM-DD-<milestone>-design.md` before implementation begins, following the same brainstorming → design → plan → implementation flow as this roadmap.

### 6.3 Open decisions (defer per-milestone; do not block the roadmap)

1. **Manual-map vs `LdrLoadDll`** for M3 injection — decided during M3 detailed spec. Recommendation: `LdrLoadDll` for the first release.
2. **Protobuf vs FlatBuffers** for M4 wire format — decided during M4 detailed spec. Recommendation: Protobuf for tooling maturity.
3. **spdlog vs custom logger** for M6 — decided during M6 detailed spec. Recommendation: spdlog.
4. **YARA runtime dependency** for M5 `ProcessSigDetector` — decided during M5 detailed spec. Recommendation: vendor libyara.

### 6.4 Risks

| Risk | Impact | Mitigation |
| ---- | ------ | ---------- |
| EV cert lead time (2–4 weeks) | Blocks M6 signed release | Order EV cert during M2 so it is in hand by the time M6 starts. |
| WHQL submission compat issue | Blocks the signed prod release | Attestation-signing pipeline is stood up in M6 as a fallback until WHQL clears. |
| Backend team delivery lag | M4 client is ready but has no receiver | `tools/mock-server/` runs indefinitely; QA smoke tests use it. |
| Windows 11 24H2 breaks the `ldr->Flags` trick or PatchGuard flags the callbacks | Kills M2 as-is | Fallback to signed driver + WHQL required, timelines shift by 1 sprint. |
| Manifest signing key leaks | Every previously shipped manifest is trustable by an attacker | Rotate to new key + emergency reissue; version-2 client refuses old-key manifests once rotated. |

## 7. What this spec does *not* decide

- The exact YARA rule set for M5 (data-only, added iteratively).
- The exact detector polling intervals (start with the values above; tune with telemetry from M4).
- The backend URL layout beyond the endpoint field in the manifest.
- The install location for per-user telemetry data (proposed `%LOCALAPPDATA%\HerculesAC\`, confirmed at M6).

Each of these is a data / tuning decision that belongs in the corresponding milestone plan, not in the roadmap.

## 8. Definition of done for the roadmap

The roadmap is complete when:

- All six milestones are shipped and tagged.
- A tag `v1.0.0` produces an EV-signed MSI on a clean CI run with no manual steps.
- The red-team suite passes with all detectors active.
- A stock Windows 11 24H2 install with Defender enabled shows zero anti-cheat–related alerts across a full game session.
- `docs/architecture.md` reflects the final state and `README.md`'s *Known issues* section is either empty or contains only issues that are explicitly deferred and tracked.
