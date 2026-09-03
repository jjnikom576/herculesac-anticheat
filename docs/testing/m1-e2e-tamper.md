# M1 end-to-end tamper test

Objective: prove that every consuming component refuses to run when the manifest,
its signature, or a referenced module has been tampered with.

## Setup

1. Build the solution (`Builder.bat`).
2. Copy the build output + `hac.manifest` + `hac.manifest.sig` to a clean deploy dir.
3. `.\tools\register-eventlog.ps1` (once, as Admin).

## Positive control

Launch `GameLauncher.exe`. HerculesAC + GameMon should start and reach the
game-launch step. No error events under Application/HerculesAC.

## Test 1 — tampered manifest bytes

Flip one byte in `hac.manifest` (any character inside the JSON payload).
Launch `GameLauncher.exe`.
Expected: HerculesAC MessageBox "hac.manifest signature is invalid…" and exit 1.
`wevtutil qe Application /q:"*[System[Provider[@Name='HerculesAC']]]" /c:1 /rd:true`
should show event ID 1001.

## Test 2 — wrong signature

Overwrite `hac.manifest.sig` with 64 zero bytes.
Expected: same result as Test 1.

## Test 3 — module hash drift

Rebuild only `GameMon` (Debug/x86) so its output changes, but keep the manifest
from the previous build.
Expected: HerculesAC exits with "Corrupted module: GameMon.aes".

## Test 4 — missing module

Delete `x64\Debug\GameMon64.aes`.
Expected: HerculesAC exits with "Corrupted module: GameMon64.aes".

Each test's fixture change must be reverted before the next test runs.

## Execution notes (this run)

This procedure was authored and reviewed against the current tree as of
`972a89a`. It was **not** run end-to-end in the environment that produced this
document, for the following reasons:

- **No VMProtect toolchain.** `VMProtect_Con.exe` (referenced by `vmp.bat`,
  see *Building* in `README.md`) is not installed in this environment, so no
  `*.aes` outputs exist and `vmp.bat` cannot run. Tests 1–4 all assume a
  fully packed deploy dir (`HerculesAC.aes`, `GameMon.aes`, `GameMon64.aes`).
- **No target game client.** The shipped default (`cstrike.exe`) is not
  present, so `GameLauncher.exe` cannot reach a real "game running" state —
  the *Positive control* step's final assertion (game launches and stays up)
  cannot be observed.
- **No Admin shell for event log registration/queries** in this environment,
  so `register-eventlog.ps1` and the `wevtutil` verification step were not
  exercised.

What **was** verified as a substitute, confirming the trust-chain code paths
exist and are wired correctly:

- The solution builds cleanly (`HerculesAC.exe`, `GameMon.exe`/`GameMon64.exe`,
  `GameProtect.dll`/`GameProtect64.dll`, `GameLauncher.exe` all present under
  `x64\Debug` / per-project `Debug` output folders from the Task 1-15 build).
- `HerculesAC/WinMain.cpp` contains the exact fail-closed strings this
  procedure checks for: `L"hac.manifest signature is invalid. Please
  reinstall the game."` (signature/tamper path) and `L"Corrupted module: " +
  mod.path` (hash/missing-module path), both routed through `WriteEventLog`
  with event ID `1001` before the `MessageBox` fires.
  See `HerculesAC/WinMain.cpp` (`FailAndExit`, `WriteEventLog`).
- The `Common/Manifest` unit tests (15/15, from Task 4-8/14) already cover
  the underlying library behaviour that Tests 1-4 exercise at the
  process level: `SignatureInvalid` (tampered bytes / wrong signature),
  `ModuleHashMismatch`, and `ModuleMissing`. Those are the same
  `ManifestError` values `HerculesAC::WinMain` branches on to produce the
  two message strings above.

**Deferred to manual QA** (tracked as follow-up, not part of this
environment's deliverable):

| Test | Status | Blocker |
| ---- | ------ | ------- |
| Positive control | Deferred | needs VMProtect-packed binaries + `cstrike.exe` |
| Test 1 — tampered manifest bytes | Deferred | needs a full deploy dir + Admin event log access |
| Test 2 — wrong signature | Deferred | same as Test 1 |
| Test 3 — module hash drift | Deferred | needs VMProtect to reproduce `.aes` naming in the error string |
| Test 4 — missing module | Deferred | needs a full deploy dir with `.aes` outputs |

A QA engineer with VMProtect Ultimate, an Admin shell, and a copy of
`cstrike.exe` should run this document verbatim on both a Windows 10 22H2
and a Windows 11 24H2 VM before M1 is considered pilot-ready, per the
original task brief (Step 3).
