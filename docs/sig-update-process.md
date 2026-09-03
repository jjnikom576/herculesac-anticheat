# HerculesAC Signature Update Process

This document describes how to maintain and extend the three signature sets used
by HerculesAC: process/window name lists, kernel driver blocklist, and hacktools
hash manifests.

---

## 1. Process and Window Name Lists

**Location:** `HerculesAC/WinMain.cpp` — `detectedProcessNames` and `detectedWindowInfo`

### When to add

- A new cheat engine, injector, or analysis tool is publicly released.
- A tool previously detected only by window title/class gains a new process name.
- A bypass technique involves a renamed binary — add both the canonical and
  common renamed variants.

### Adding a process name

```cpp
// in detectedProcessNames[]
"mynewcheat.exe",
```

Rules:
- All lowercase.
- Match the exact default filename the tool ships with.
- If the tool ships under multiple names, add each name on its own line.
- Do not add generic names (`cmd.exe`, `python.exe`) — false-positive risk is
  too high.

### Adding a window detection entry

```cpp
// in detectedWindowInfo[]
{ L"ExactWindowClass", L"Partial Title Substring" },
```

Rules:
- WindowClass must be the exact `lpszClassName` the tool registers.
- Title is a substring match (case-sensitive). Use the shortest unique fragment.
- Leave either field as `L""` to match any class or any title respectively.
- Verify with Spy++ or `GetClassName` / `GetWindowText` on a live instance.

### Verification

1. Run the game with `HerculesAC.exe` attached.
2. Launch the tool under test.
3. Confirm a `DetectedHackTool` (Kind=ProcName or Kind=WinName) event appears in
   the telemetry dashboard (`/dashboard`) within one poll cycle (5 seconds).

---

## 2. Kernel Driver Blocklist

**Location:** `HerculesAC/Detectors/DriverEnumDetector.cpp` — `kBadDrivers[]`

### When to add

- A BYOVD (Bring Your Own Vulnerable Driver) technique targets a newly disclosed
  driver CVE.
- A cheat tool ships a new kernel driver that bypasses existing protections.
- A security researcher publishes a PoC that loads a specific `.sys` file.

### Adding a driver

```cpp
"newbaddriver.sys",     // Short description — CVE or tool name
```

Rules:
- All lowercase, just the filename with `.sys` extension — no path.
- Add a trailing comment with the relevant CVE or the cheat/tool name.
- Keep entries alphabetically ordered within their category comment block.

### Finding the right filename

```powershell
# List currently loaded drivers
Get-CimInstance Win32_SystemDriver | Select-Object Name, PathName | Sort-Object Name
```

Or from the NtQuerySystemInformation output logged by DriverEnumDetector itself
(Kind=KnownBadDriver events include the `driver` field in evidence_json).

### Reference: BYOVD tracker

Consult the [loldrivers.io](https://www.loldrivers.io/) database for disclosed
vulnerable/malicious drivers before adding a new entry.

### Verification

Load the driver in a test VM, then confirm Kind=KnownBadDriver appears in
telemetry. The event's evidence_json should contain `driver`, `base`, and `size`.

---

## 3. Hacktools Hash Manifest (`.hac` files)

**Location:** `assets/hacktools-sig/` (Git LFS tracked) and `Manifest` library.

The manifest is a JSON file signed with the dev Ed25519 key. Each entry
describes a known hacktools binary by SHA-256 hash.

### Workflow

#### 3a. Collect the binary

Obtain the hacktools binary from a sandboxed environment. Do **not** run it on
a development machine without isolation.

#### 3b. Compute the hash

```powershell
(Get-FileHash "cheat.exe" -Algorithm SHA256).Hash.ToLower()
```

#### 3c. Edit the manifest JSON

```json
{
  "version": 3,
  "entries": [
    {
      "sha256": "aabbcc...",
      "name":   "NewCheat v2.1",
      "kind":   "ModuleHash",
      "severity": 3
    }
  ]
}
```

Fields:
| Field | Values |
|-------|--------|
| `sha256` | lowercase hex, 64 chars |
| `name` | human-readable label (shown in evidence_json) |
| `kind` | `"ModuleHash"` for EXE/DLL hashes |
| `severity` | 1=Info, 2=Warn, **3=Critical** |

#### 3d. Re-sign the manifest

```powershell
# sign-manifest.exe is built by the Manifest project
.\tools\sign-manifest.exe `
    --key tools\keys\hac-dev.private.key `
    --manifest assets\hacktools-sig\hacktools.json `
    --out     assets\hacktools-sig\hacktools.hac
```

The `.hac` file is the signed binary blob loaded at runtime by `Manifest::Load`.

> **SECURITY:** `hac-dev.private.key` is gitignored and must NEVER be committed.
> Only the public key hex (`046070fafe0c77fb20be806702cd95e76b1e6074cb1da2c75aacd990c21fc456`)
> is compiled into the binary via `EmbeddedPublicKey.h`.

#### 3e. Commit and push

```bash
git add assets/hacktools-sig/hacktools.json assets/hacktools-sig/hacktools.hac
git commit -m "sig: add <tool name> to hacktools manifest (v<N>)"
git push
```

The `.hac` binary is tracked by Git LFS (see `.gitattributes`).

#### 3f. Verification

Start a test session, confirm Kind=ModuleHash detection fires within one
poll cycle when the added binary hash is loaded into the process.

---

## 4. Release Checklist

Before tagging a new sig-update release (`sig-vX.Y`):

- [ ] All three signature sets have been updated.
- [ ] Each entry has been verified with a live test or sandbox run.
- [ ] `hacktools.hac` re-signed with the current dev key.
- [ ] Private key confirmed absent from `git status` / `git log`.
- [ ] Telemetry dashboard shows expected detection events.
- [ ] PR reviewed by at least one team member.

---

## 5. Removing False Positives

If a legitimate tool is falsely detected:

1. Remove the exact entry from the relevant list.
2. If a process name matches a legitimate binary, prefer a window-class based
   entry with a more specific title instead.
3. Document the removal in the commit message with the reason.
4. Re-sign the hacktools manifest if a hash entry was removed.
