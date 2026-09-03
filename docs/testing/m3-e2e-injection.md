# M3 End-to-End: Targeted Injection Validation

Milestone 3 replaces the global `WH_GETMESSAGE` hook mechanism with direct,
per-process `CreateRemoteThread + LoadLibraryW` injection, scoped strictly to
the game process.  This document describes how to validate the change
manually on a development machine.

## What changed

| Before M3 | After M3 |
|---|---|
| `GameMon` called `SetWindowsHookEx(WH_GETMESSAGE)` | `GameMon::SetupHook` / `UnHook` are no-ops |
| `GameProtect.dll` broadcast into every desktop UI process | `GameProtect.dll` injected only into the game process PID |
| `IsWhitelistCurrentProcess` hashed the host binary to decide whether to hook | Removed — no whitelisting needed because we only land in the game |
| Kernel `PsSetCreateThreadNotifyRoutine` absent | Added as monitoring hook (logs new threads in protected PIDs) |

## Prerequisites

- Windows 10/11 x64, test-signing enabled or kernel driver signed
- `herculeskernel.sys` loaded (see `docs/testing/m2-e2e-driver-load.md`)
- Debug build of `HerculesAC.exe` + `GameProtect64.dll` in same directory
- `hac.manifest` + `hac.manifest.sig` present alongside the binaries
- Target game binary available (the packaged build uses `cstrike.exe`)

## Test 1 — GameProtect not present in unrelated processes

**Goal:** confirm the global hook is gone.

1. Open Process Monitor (Sysinternals), add filter: `Path contains GameProtect`, Operation = `Load Image`.
2. Launch `HerculesAC.exe`.  Note the game PID in debug output.
3. In Process Monitor, observe that `GameProtect64.dll` loads **only** into the game PID.
4. Open any other UI process (Notepad, Calculator).  Confirm no `GameProtect64.dll` load event appears.

**Pass criteria:** `GameProtect64.dll` load events are limited to the single game process PID.

## Test 2 — Targeted injection succeeds

**Goal:** confirm `InjectDll` succeeds and hooks are active in the game process.

1. Attach a debugger (WinDbg or x64dbg) to the game process after `HerculesAC.exe` starts it.
2. In the Modules list, confirm `GameProtect64.dll` is loaded.
3. Set a breakpoint on `ReadProcessMemory` (in kernel32) from within the game process.
4. From a separate process, call `ReadProcessMemory` targeting the game PID.
5. Confirm the breakpoint hits — the Detours hook in `GameProtect` intercepts it.

**Pass criteria:** `GameProtect64.dll` listed in game process modules; RPM hook fires.

## Test 3 — Kernel thread notify fires

**Goal:** confirm `PsSetCreateThreadNotifyRoutine` callback logs new threads.

1. Enable kernel debug output: run `DbgView` (Sysinternals) as administrator with `Capture Kernel` enabled.
2. Load `herculeskernel.sys` and register the game PID via the IOCTL
   `IOCTL_START_PROTECT` (see `m2-e2e-driver-load.md`).
3. Inside the game process, create a new thread (e.g., via `CreateThread` from a test injector).
4. Observe DbgView output containing:
   ```
   ThreadNotify: new thread in protected PID <pid>
   ```

**Pass criteria:** kernel log line appears for every new thread created inside the protected PID.

## Test 4 — Unrelated PIDs do not appear in thread notify log

**Goal:** no false positives from `OnThreadNotify`.

1. With the same DbgView session from Test 3, create threads in an *unrelated* process (e.g., Notepad).
2. Confirm no `ThreadNotify: new thread in protected PID` line is logged for the unrelated PID.

**Pass criteria:** zero spurious log lines for unregistered PIDs.

## Regression check

Run the existing M2 IOCTL smoke-tests from `docs/testing/m2-e2e-driver-load.md`
to confirm the IOCTL surface (`IOCTL_START_PROTECT`, `IOCTL_STOP_PROTECT`,
`IOCTL_QUERY_STATUS`) is unaffected by M3 changes.

## Known limitations

- `InjectDll` waits up to 10 s (`WaitForSingleObject`) for `LoadLibraryW`
  to complete before returning.  If the game process applies anti-injection
  mitigations the remote thread will not start and `InjectDll` returns `false`;
  HerculesAC logs this silently for now (M4 will wire proper error reporting).
- `PsSetCreateThreadNotifyRoutine` is limited to 64 registered callbacks
  system-wide.  If this slot is exhausted at driver load time,
  `InitThreadNotify` logs a failure NTSTATUS and continues without the
  thread monitor; the rest of the driver remains functional.
