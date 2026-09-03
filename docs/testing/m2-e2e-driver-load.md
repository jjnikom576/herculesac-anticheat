# M2 end-to-end driver load test

Objective: prove that the kernel driver loads correctly, the IOCTL surface works, PID
protection is dynamic (not hardcoded), and Ob callbacks strip access rights from
protected PIDs.

## Prerequisites

- Windows 10/11 x64 in **test-signing mode** (`bcdedit /set testsigning on`, reboot)
- The driver is test-signed (Builder.bat does this automatically)
- Admin rights in the test VM

## 1. Build

```bat
Builder.bat
```

Kernel output: `herculeskernel\x64\Debug\herculeskernel.sys`

## 2. Install and start the driver

```bat
sc create HerculesKernel type= kernel binPath= "<abs-path>\herculeskernel\x64\Debug\herculeskernel.sys"
sc start HerculesKernel
```

Expected: `sc start` exits 0 and `DriverEntry` log appears in DebugView / WinDbg.

## 3. Verify the device node exists

```bat
dir \\.\HerculesAC
```

Expected: the device opens (no "file not found").

## 4. Smoke-test IOCTL_QUERY_STATUS

Use a PowerShell one-liner (admin):

```powershell
$dev = [System.IO.File]::Open('\\.\HerculesAC',
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::ReadWrite,
    [System.IO.FileShare]::None)

# IOCTL_QUERY_STATUS = CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, 0x805, METHOD_BUFFERED=0, FILE_ANY_ACCESS=0)
# = (0x22 << 16) | (0 << 14) | (0x805 << 2) | 0 = 0x00220014 ... recalculate:
# CTL_CODE(0x22, 0x805, 0, 0) = (0x22 << 16) | (0x805 << 2) = 0x00222014
$code = 0x00222014
$out  = New-Object byte[] 8
$dev.Flush()
# DeviceIoControl via P/Invoke omitted — use a small C# snippet or winobj/ioctlbg for quick check
$dev.Close()
```

Alternatively compile and run `tools\query-status.c` (if provided) or use a kernel debugger:

```
kd> dt hac::g_pidTable
```

Expected: `active_protection_count = 0`, `driver_version = 0x00020000`.

## 5. Register a PID and verify Ob callback protection

1. Launch Notepad: note its PID (e.g. 1234).
2. Call `IOCTL_START_PROTECT` with that PID (requires SeDebugPrivilege / admin).
3. From a second process, attempt `OpenProcess(PROCESS_VM_READ, FALSE, 1234)`.

Expected: `PROCESS_VM_READ` is stripped — the handle is opened but `ReadProcessMemory`
returns `ERROR_ACCESS_DENIED` (5).

4. Call `IOCTL_STOP_PROTECT` for the same PID.
5. Repeat step 3 — `ReadProcessMemory` succeeds.

## 6. Verify HerculesAC auto-registers the game PID

1. Launch `HerculesAC.exe <GamePath>` (admin, SeDebugPrivilege).
2. After the starter process launches, call `IOCTL_QUERY_STATUS`.

Expected: `active_protection_count >= 1`.

## 7. Unload the driver

```bat
sc stop HerculesKernel
sc delete HerculesKernel
```

Expected: `DriverUnload` log appears; device node `\\.\HerculesAC` disappears.

## 8. Regression: no PatchGuard trip

Run the system under load for 15 minutes with the driver loaded. Expected: no
`0x109 CRITICAL_STRUCTURE_CORRUPTION` BSOD (Ob callbacks at altitude 321000 use a
supported API and do not touch kernel text).
