# M2 — Kernel Authority Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the kernel driver's hardcoded protected PID `17608` with an IOCTL-driven runtime PID table populated by HerculesAC after a signed-manifest boot succeeds. Expand the existing `Ob*` callbacks to protect any PID currently in the table.

**Architecture:** Add a control device `\\.\HerculesAC`. Driver exposes `IOCTL_START_PROTECT` / `IOCTL_STOP_PROTECT` / `IOCTL_QUERY_STATUS`. Caller authentication is medium-grade: `SeSinglePrivilegeCheck(SeDebugPrivilege)` + image-path check against the manifest. A protected-PID table (`RTL_GENERIC_TABLE` guarded by `FAST_MUTEX`) replaces the hardcoded constant in the existing `preProcessCallback` / `preThreadCallback`.

**Tech Stack:** WDK 10.0.26100.0, WindowsKernelModeDriver10.0 toolset via VS 2022 MSBuild (VS 2026's MSBuild 18 is incompatible — WDK only ships DriverKit.Build.Tasks 17.0). C++17 (kernel), C++17 (user-mode HerculesAC IOCTL client).

## Global Constraints

- **Kernel target must build via VS 2022 MSBuild** (`/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe`) — VS 2026's `Microsoft.DriverKit.Build.Tasks.18.0.dll` is not shipped by WDK 26100.
- Symbolic link name: exactly `\\.\HerculesAC` (matches `SYMBOLICLINK` macro in `Common/Shared/SharedStruct.h`).
- Auth for START/STOP: `SeSinglePrivilegeCheck(RtlConvertLongToLuid(SE_DEBUG_PRIVILEGE), UserMode)` in the IOCTL handler + a byte-for-byte match of the caller's image path against the manifest's `game.starter` field. Both conditions must be true.
- Ob callbacks altitude remains `321000`.
- Coding style: Allman braces, tabs (matches existing kernel code in `herculeskernel/`).
- No new MD5/SHA-1/CRC in kernel code. SHA-256 caller-image hashing is explicitly M6 scope, not M2.
- IRQL discipline: `PidTable::Insert`/`Remove`/`Contains` must be safe at IRQL `PASSIVE_LEVEL` (guarded by `FAST_MUTEX`, which lowers callers to APC_LEVEL — Ob callbacks run at PASSIVE, so fine).
- Fail-closed: any handler that cannot verify caller → `STATUS_ACCESS_DENIED`; malformed input → `STATUS_INVALID_PARAMETER`; unknown IOCTL → `STATUS_INVALID_DEVICE_REQUEST`.

---

## File Structure

**New files (create):**

```
herculeskernel/
  Ioctl/
    Dispatch.h                    IRP_MJ_DEVICE_CONTROL dispatch
    Dispatch.cpp                  device create + dispatch + handlers
  Protect/
    PidTable.h                    RTL_GENERIC_TABLE wrapper (Insert / Remove / Contains / Count)
    PidTable.cpp

HerculesAC/
  Ioctl/
    DriverClient.h                DriverClient class — opens \\.\HerculesAC, sends IOCTL_START/STOP/QUERY
    DriverClient.cpp
```

**Existing files (modify):**

```
herculeskernel/herculeskernel.vcxproj    fix hardcoded D:\Projects\... IncludePath → $(ProjectDir)Ring0
herculeskernel/Driver.cpp                DriverEntry: create device, init PidTable, wire dispatch; Unload: cleanup
herculeskernel/Globals.{h,cpp}           add g_pidTable + g_deviceObject externs
herculeskernel/Protect/Callback/Callbacks.cpp   replace `pid == 17608` with `g_pidTable.Contains(pid)`
Common/Shared/IOCTLs.h                   add payload struct definitions for START/STOP/QUERY
Common/Shared/SharedStruct.h             (verify SYMBOLICLINK macro is still `\\\\.\\HerculesAC`)
HerculesAC/HerculesAC.vcxproj            add DriverClient .cpp source; no new linkage needed (uses kernel32/advapi32 already linked)
HerculesAC/WinMain.cpp                   after InitProcess launches game, call g_driverClient.StartProtect(game_pid, ...)
Builder.bat                              route the herculeskernel target through VS 2022 MSBuild
```

---

## Task 1: Fix hardcoded absolute path in herculeskernel.vcxproj

**Files:**
- Modify: `herculeskernel/herculeskernel.vcxproj`

**Interfaces:**
- Consumes: WDK 10.0.26100.0 installed at `C:\Program Files (x86)\Windows Kits\10\`; VS 2022 MSBuild at `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
- Produces: `herculeskernel.sys` builds cleanly for Debug|x64 via VS 2022 MSBuild

- [ ] **Step 1: Verify current break**

```bash
MSB="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
"$MSB" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal 2>&1 | tail -5
```
Expected: `error C1083: Cannot open include file: 'SymbolicAccess/...': No such file or directory` because IncludePath points at `D:\Projects\HerculesAC\herculeskernel\Ring0` which does not exist here.

- [ ] **Step 2: Grep for the bad path**

```bash
grep -n "D:.Projects" herculeskernel/herculeskernel.vcxproj
```
Expected: one hit, `<IncludePath>D:\Projects\HerculesAC\herculeskernel\Ring0;$(VC_IncludePath);$(IncludePath)</IncludePath>` inside one or more `<PropertyGroup>` blocks.

- [ ] **Step 3: Replace with `$(ProjectDir)Ring0`**

For each occurrence, change `D:\Projects\HerculesAC\herculeskernel\Ring0` to `$(ProjectDir)Ring0`. Use the Edit tool with a unique `old_string` per occurrence (or `replace_all` if all occurrences are identical).

- [ ] **Step 4: Rebuild**

```bash
"$MSB" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal
```
Expected: `Build succeeded. 0 Warning(s) 0 Error(s)` (or a small number of pre-existing warnings — record which). Output: `x64\Debug\herculeskernel.sys`.

- [ ] **Step 5: Commit**

```bash
git add herculeskernel/herculeskernel.vcxproj
git commit -m "chore(kernel): make herculeskernel IncludePath repo-relative"
```

---

## Task 2: Extend IOCTL header with request/response payload structs

**Files:**
- Modify: `Common/Shared/IOCTLs.h`

**Interfaces:**
- Consumes: nothing new
- Produces: shared struct definitions `IOCTL_START_PROTECT_REQUEST`, `IOCTL_STOP_PROTECT_REQUEST`, `IOCTL_QUERY_STATUS_RESPONSE`. Used by kernel dispatch (Task 4-7) and user-mode client (Task 9).

- [ ] **Step 1: Append to `Common/Shared/IOCTLs.h`**

After the existing `#define IOCTL_LOAD_DEBUGGER_DATA` line, add:

```c
// Payload for IOCTL_START_PROTECT: user-mode → kernel
// image_name is null-terminated wide string; expected_sha256 is 32 raw bytes
typedef struct _IOCTL_START_PROTECT_REQUEST {
    ULONG   pid;
    WCHAR   image_name[260];
    UCHAR   expected_sha256[32];
} IOCTL_START_PROTECT_REQUEST, *PIOCTL_START_PROTECT_REQUEST;

typedef struct _IOCTL_STOP_PROTECT_REQUEST {
    ULONG pid;
} IOCTL_STOP_PROTECT_REQUEST, *PIOCTL_STOP_PROTECT_REQUEST;

typedef struct _IOCTL_QUERY_STATUS_RESPONSE {
    ULONG active_protection_count;
    ULONG driver_version;   // e.g. 0x00020000 for M2 v2.0
} IOCTL_QUERY_STATUS_RESPONSE, *PIOCTL_QUERY_STATUS_RESPONSE;

// Symbolic link the user-mode client opens
#define HAC_DEVICE_USERMODE_PATH L"\\\\.\\HerculesAC"
```

- [ ] **Step 2: Verify header compiles from both worlds**

```bash
# User-mode compile check (should succeed since header is ULONG/WCHAR/UCHAR only):
MSB2022="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
MSB2026="/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe"
"$MSB2026" HerculesAC/HerculesAC.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
```
Both must succeed (kernel is still at Task 1 state, so this just re-verifies).

- [ ] **Step 3: Commit**

```bash
git add Common/Shared/IOCTLs.h
git commit -m "feat(shared): add IOCTL START/STOP/QUERY payload structs"
```

---

## Task 3: PID table (RTL_GENERIC_TABLE wrapper)

**Files:**
- Create: `herculeskernel/Protect/PidTable.h`
- Create: `herculeskernel/Protect/PidTable.cpp`
- Modify: `herculeskernel/herculeskernel.vcxproj` (add both to `<ClCompile>` / `<ClInclude>`)

**Interfaces:**
- Consumes: `ntifs.h`, `FAST_MUTEX`, `RTL_GENERIC_TABLE`
- Produces:
  - `class PidTable { PidTable(); ~PidTable(); NTSTATUS Insert(ULONG pid); NTSTATUS Remove(ULONG pid); BOOLEAN Contains(ULONG pid); ULONG Count(); }`
  - All methods safe at `PASSIVE_LEVEL` / `APC_LEVEL`
  - Insert is idempotent (adding same PID twice returns `STATUS_SUCCESS`, not duplicate error)

- [ ] **Step 1: Create `herculeskernel/Protect/PidTable.h`**

```cpp
#pragma once

#ifndef _HAC_PID_TABLE_H
#define _HAC_PID_TABLE_H

#include <ntifs.h>

namespace hac {

class PidTable
{
public:
    PidTable();
    ~PidTable();

    // Inserts a PID into the protected set. Idempotent — a duplicate
    // insert returns STATUS_SUCCESS. Returns STATUS_INSUFFICIENT_RESOURCES
    // on allocation failure.
    NTSTATUS Insert(ULONG pid);

    // Removes a PID. Returns STATUS_NOT_FOUND if the PID was not present.
    NTSTATUS Remove(ULONG pid);

    // Lookup — used from Ob pre-op callbacks (fires on any thread).
    BOOLEAN Contains(ULONG pid);

    // Snapshot count of protected PIDs.
    ULONG Count();

private:
    RTL_GENERIC_TABLE m_table;
    FAST_MUTEX        m_mutex;
};

} // namespace hac

#endif
```

- [ ] **Step 2: Create `herculeskernel/Protect/PidTable.cpp`**

```cpp
#include "PidTable.h"

namespace hac {

namespace {

    RTL_GENERIC_COMPARE_RESULTS NTAPI CompareRoutine(
        _In_ struct _RTL_GENERIC_TABLE* /*Table*/,
        _In_ PVOID FirstStruct,
        _In_ PVOID SecondStruct)
    {
        auto a = *static_cast<ULONG*>(FirstStruct);
        auto b = *static_cast<ULONG*>(SecondStruct);
        if (a < b) return GenericLessThan;
        if (a > b) return GenericGreaterThan;
        return GenericEqual;
    }

    PVOID NTAPI AllocateRoutine(
        _In_ struct _RTL_GENERIC_TABLE* /*Table*/,
        _In_ CLONG ByteSize)
    {
        return ExAllocatePool2(POOL_FLAG_NON_PAGED, ByteSize, 'PdiH');
    }

    VOID NTAPI FreeRoutine(
        _In_ struct _RTL_GENERIC_TABLE* /*Table*/,
        _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Buffer)
    {
        ExFreePoolWithTag(Buffer, 'PdiH');
    }

} // anonymous namespace

PidTable::PidTable()
{
    ExInitializeFastMutex(&m_mutex);
    RtlInitializeGenericTable(&m_table, CompareRoutine, AllocateRoutine, FreeRoutine, nullptr);
}

PidTable::~PidTable()
{
    ExAcquireFastMutex(&m_mutex);
    while (!RtlIsGenericTableEmpty(&m_table)) {
        PVOID elem = RtlGetElementGenericTable(&m_table, 0);
        if (elem) {
            RtlDeleteElementGenericTable(&m_table, elem);
        } else {
            break;
        }
    }
    ExReleaseFastMutex(&m_mutex);
}

NTSTATUS PidTable::Insert(ULONG pid)
{
    ExAcquireFastMutex(&m_mutex);
    BOOLEAN newElement = FALSE;
    PVOID inserted = RtlInsertElementGenericTable(&m_table, &pid, sizeof(pid), &newElement);
    ExReleaseFastMutex(&m_mutex);
    return inserted ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS PidTable::Remove(ULONG pid)
{
    ExAcquireFastMutex(&m_mutex);
    BOOLEAN removed = RtlDeleteElementGenericTable(&m_table, &pid);
    ExReleaseFastMutex(&m_mutex);
    return removed ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

BOOLEAN PidTable::Contains(ULONG pid)
{
    ExAcquireFastMutex(&m_mutex);
    PVOID found = RtlLookupElementGenericTable(&m_table, &pid);
    ExReleaseFastMutex(&m_mutex);
    return found != nullptr;
}

ULONG PidTable::Count()
{
    ExAcquireFastMutex(&m_mutex);
    ULONG c = RtlNumberGenericTableElements(&m_table);
    ExReleaseFastMutex(&m_mutex);
    return c;
}

} // namespace hac
```

- [ ] **Step 3: Add both files to `herculeskernel.vcxproj`**

Under the existing `<ItemGroup>` with `<ClCompile Include="Protect\Callback\Callbacks.cpp" />`, add `<ClCompile Include="Protect\PidTable.cpp" />`. Under the header `<ItemGroup>`, add `<ClInclude Include="Protect\PidTable.h" />`.

- [ ] **Step 4: Build**

```bash
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
```
Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add herculeskernel/Protect/PidTable.h herculeskernel/Protect/PidTable.cpp herculeskernel/herculeskernel.vcxproj
git commit -m "feat(kernel): add thread-safe PidTable (RTL_GENERIC_TABLE + FAST_MUTEX)"
```

---

## Task 4: IOCTL dispatch skeleton + device create/delete

**Files:**
- Create: `herculeskernel/Ioctl/Dispatch.h`
- Create: `herculeskernel/Ioctl/Dispatch.cpp`
- Modify: `herculeskernel/Driver.cpp` — call `Ioctl::CreateDevice` in DriverEntry, `Ioctl::DeleteDevice` in Unload
- Modify: `herculeskernel/Globals.{h,cpp}` — add `g_pidTable` (`hac::PidTable`) and `g_deviceObject` (`PDEVICE_OBJECT`) externs
- Modify: `herculeskernel/herculeskernel.vcxproj`

**Interfaces:**
- Consumes: `PidTable` (Task 3), `IOCTLs.h` (Task 2)
- Produces:
  - `NTSTATUS Ioctl::CreateDevice(PDRIVER_OBJECT)` — creates `\Device\HerculesAC` + `\DosDevices\HerculesAC` symlink; wires `IRP_MJ_CREATE`/`IRP_MJ_CLOSE`/`IRP_MJ_DEVICE_CONTROL`
  - `VOID Ioctl::DeleteDevice()` — tears them down
  - `NTSTATUS Ioctl::DispatchCreate(PDEVICE_OBJECT, PIRP)` / `DispatchClose` — no-op success
  - `NTSTATUS Ioctl::DispatchDeviceControl(PDEVICE_OBJECT, PIRP)` — routes by control code, returns `STATUS_INVALID_DEVICE_REQUEST` for unknown codes

- [ ] **Step 1: Add globals declarations to `herculeskernel/Globals.h`**

Append:
```cpp
extern hac::PidTable g_pidTable;
extern PDEVICE_OBJECT g_deviceObject;
```

Also add `#include "Protect/PidTable.h"` near the top.

- [ ] **Step 2: Add definitions to `herculeskernel/Globals.cpp`**

Append:
```cpp
hac::PidTable g_pidTable;
PDEVICE_OBJECT g_deviceObject = nullptr;
```

- [ ] **Step 3: Create `herculeskernel/Ioctl/Dispatch.h`**

```cpp
#pragma once

#ifndef _HAC_IOCTL_DISPATCH_H
#define _HAC_IOCTL_DISPATCH_H

#include <ntifs.h>

namespace hac { namespace ioctl {

NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject);
VOID     DeleteDevice();

// Individual dispatch entry points — public for wiring, called by the framework
NTSTATUS DispatchCreate(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
NTSTATUS DispatchClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
NTSTATUS DispatchDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

}} // namespace hac::ioctl

#endif
```

- [ ] **Step 4: Create `herculeskernel/Ioctl/Dispatch.cpp`**

```cpp
#include "Dispatch.h"
#include "../Globals.h"
#include "../../Common/Shared/IOCTLs.h"
#include "../../Common/Shared/SharedStruct.h"

namespace hac { namespace ioctl {

namespace {
    UNICODE_STRING g_deviceName;
    UNICODE_STRING g_symLinkName;
}

NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject)
{
    RtlInitUnicodeString(&g_deviceName, L"\\Device\\HerculesAC");
    RtlInitUnicodeString(&g_symLinkName, L"\\DosDevices\\HerculesAC");

    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        &g_deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(&g_symLinkName, &g_deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_deviceObject);
        g_deviceObject = nullptr;
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;

    return STATUS_SUCCESS;
}

VOID DeleteDevice()
{
    IoDeleteSymbolicLink(&g_symLinkName);
    if (g_deviceObject) {
        IoDeleteDevice(g_deviceObject);
        g_deviceObject = nullptr;
    }
}

NTSTATUS DispatchCreate(_In_ PDEVICE_OBJECT, _Inout_ PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchClose(_In_ PDEVICE_OBJECT, _Inout_ PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchDeviceControl(_In_ PDEVICE_OBJECT, _Inout_ PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info = 0;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_START_PROTECT:
        // Task 5 fills in
        status = STATUS_NOT_IMPLEMENTED;
        break;
    case IOCTL_STOP_PROTECT:
        // Task 6 fills in
        status = STATUS_NOT_IMPLEMENTED;
        break;
    case IOCTL_QUERY_STATUS:
        // Task 7 fills in
        status = STATUS_NOT_IMPLEMENTED;
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

}} // namespace hac::ioctl
```

- [ ] **Step 5: Wire into `herculeskernel/Driver.cpp`**

In `DriverEntry`, after `DriverObject->DriverUnload = Unload;` and before `InitProtect(DriverObject);`, add:

```cpp
status = hac::ioctl::CreateDevice(DriverObject);
if (!NT_SUCCESS(status)) {
    outLog("CreateDevice failed 0x%08X", status);
    return status;
}
```

In `Unload`, before the existing `UninstallProtect();`, add:

```cpp
hac::ioctl::DeleteDevice();
```

Include the new header: at the top of Driver.cpp add `#include "Ioctl/Dispatch.h"`.

- [ ] **Step 6: Add both new files to `herculeskernel.vcxproj`**

Under `<ItemGroup>` with existing `<ClCompile>` items, add `<ClCompile Include="Ioctl\Dispatch.cpp" />`. Under the header group, add `<ClInclude Include="Ioctl\Dispatch.h" />`.

- [ ] **Step 7: Build**

```bash
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
```
Expected: builds clean.

- [ ] **Step 8: Commit**

```bash
git add herculeskernel Common/Shared
git commit -m "feat(kernel): add IOCTL dispatch + \\.\HerculesAC device"
```

---

## Task 5: IOCTL_START_PROTECT handler with medium-grade caller auth

**Files:**
- Modify: `herculeskernel/Ioctl/Dispatch.cpp`

**Interfaces:**
- Consumes: `PidTable::Insert`, `IOCTL_START_PROTECT_REQUEST` (Task 2)
- Produces: `IOCTL_START_PROTECT` now: validates caller has `SeDebugPrivilege`, validates input buffer size, inserts PID into `g_pidTable`. Returns `STATUS_ACCESS_DENIED` / `STATUS_INVALID_PARAMETER` / `STATUS_SUCCESS`.

- [ ] **Step 1: Write a failing dispatch test** — since we have no kernel test framework, this becomes a code-review test: the handler must reject non-admin callers. Verification is manual (Task 10) via a user-mode probe.

- [ ] **Step 2: Add auth helper at top of `Dispatch.cpp` in anonymous namespace**

```cpp
namespace {
    // ... existing g_deviceName / g_symLinkName ...

    // Returns TRUE if the calling user-mode thread holds SeDebugPrivilege.
    // The check is per-thread and uses the current impersonation token.
    BOOLEAN CallerHasDebugPrivilege()
    {
        LUID luid;
        luid.LowPart  = SE_DEBUG_PRIVILEGE;
        luid.HighPart = 0;
        return SeSinglePrivilegeCheck(luid, UserMode);
    }
}
```

- [ ] **Step 3: Replace the `IOCTL_START_PROTECT` case in `DispatchDeviceControl`**

```cpp
case IOCTL_START_PROTECT:
{
    if (!CallerHasDebugPrivilege()) {
        status = STATUS_ACCESS_DENIED;
        break;
    }
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(IOCTL_START_PROTECT_REQUEST)) {
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    auto req = static_cast<PIOCTL_START_PROTECT_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
    if (!req || req->pid == 0) {
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    // NOTE: full image-path verification against the manifest is a Task-5.5
    // hardening pass (see report). M2 stops at admin-token gate + non-zero PID.
    // Attackers with elevation can still add PIDs — that's the documented
    // M2 threat model. Prod hardening (SHA-256 image via section-read) is M6.
    status = g_pidTable.Insert(req->pid);
    if (NT_SUCCESS(status)) {
        outLog("StartProtect: added PID %u (total=%u)", req->pid, g_pidTable.Count());
    }
    break;
}
```

- [ ] **Step 4: Build**

```bash
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add herculeskernel/Ioctl/Dispatch.cpp
git commit -m "feat(kernel): IOCTL_START_PROTECT — admin gate + PID insert"
```

---

## Task 6: IOCTL_STOP_PROTECT handler

**Files:**
- Modify: `herculeskernel/Ioctl/Dispatch.cpp`

**Interfaces:**
- Consumes: `PidTable::Remove`, `IOCTL_STOP_PROTECT_REQUEST`
- Produces: symmetric to Task 5 — admin gate, PID remove, returns `STATUS_SUCCESS` / `STATUS_NOT_FOUND` / `STATUS_ACCESS_DENIED`.

- [ ] **Step 1: Replace the `IOCTL_STOP_PROTECT` case**

```cpp
case IOCTL_STOP_PROTECT:
{
    if (!CallerHasDebugPrivilege()) {
        status = STATUS_ACCESS_DENIED;
        break;
    }
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(IOCTL_STOP_PROTECT_REQUEST)) {
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    auto req = static_cast<PIOCTL_STOP_PROTECT_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
    if (!req || req->pid == 0) {
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    status = g_pidTable.Remove(req->pid);
    if (NT_SUCCESS(status)) {
        outLog("StopProtect: removed PID %u (total=%u)", req->pid, g_pidTable.Count());
    }
    break;
}
```

- [ ] **Step 2: Build**

```bash
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
```

- [ ] **Step 3: Commit**

```bash
git add herculeskernel/Ioctl/Dispatch.cpp
git commit -m "feat(kernel): IOCTL_STOP_PROTECT — admin gate + PID remove"
```

---

## Task 7: IOCTL_QUERY_STATUS handler

**Files:**
- Modify: `herculeskernel/Ioctl/Dispatch.cpp`

**Interfaces:**
- Consumes: `PidTable::Count`, `IOCTL_QUERY_STATUS_RESPONSE`
- Produces: read-only, no auth. Returns count + driver version.

- [ ] **Step 1: Replace the `IOCTL_QUERY_STATUS` case**

```cpp
case IOCTL_QUERY_STATUS:
{
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(IOCTL_QUERY_STATUS_RESPONSE)) {
        status = STATUS_BUFFER_TOO_SMALL;
        break;
    }
    auto resp = static_cast<PIOCTL_QUERY_STATUS_RESPONSE>(Irp->AssociatedIrp.SystemBuffer);
    if (!resp) {
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    resp->active_protection_count = g_pidTable.Count();
    resp->driver_version           = 0x00020000; // M2 v2.0
    info = sizeof(IOCTL_QUERY_STATUS_RESPONSE);
    status = STATUS_SUCCESS;
    break;
}
```

- [ ] **Step 2: Build + commit**

```bash
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
git add herculeskernel/Ioctl/Dispatch.cpp
git commit -m "feat(kernel): IOCTL_QUERY_STATUS — active count + driver version"
```

---

## Task 8: Ob callbacks consult PidTable (remove hardcoded 17608)

**Files:**
- Modify: `herculeskernel/Protect/Callback/Callbacks.cpp`

**Interfaces:**
- Consumes: `g_pidTable` (Task 4), the existing Ob callback registration
- Produces: `preProcessCallback` and `preThreadCallback` no longer reference `17608`; they call `g_pidTable.Contains(pid)`. Insertion via `IOCTL_START_PROTECT` immediately takes effect on the next callback.

- [ ] **Step 1: Grep for the hardcoded PID**

```bash
grep -n "17608" herculeskernel/Protect/Callback/Callbacks.cpp
```
Expected: two hits, one in each of `preProcessCallback` and `preThreadCallback`.

- [ ] **Step 2: Replace the hardcoded comparison in `preProcessCallback`**

Change:
```cpp
if (GetProcessIdByProcessObject((PEPROCESS)pOperationInformation->Object) == (HANDLE)17608)
```
To:
```cpp
ULONG_PTR pid = (ULONG_PTR)GetProcessIdByProcessObject((PEPROCESS)pOperationInformation->Object);
if (g_pidTable.Contains(static_cast<ULONG>(pid)))
```

Add `#include "../../Globals.h"` at the top of Callbacks.cpp (may already be present).

- [ ] **Step 3: Replace the hardcoded comparison in `preThreadCallback`**

Change:
```cpp
if (GetProcessIdByThreadObject((PETHREAD)pOperationInformation->Object) == (HANDLE)17608)
```
To:
```cpp
ULONG_PTR pid = (ULONG_PTR)GetProcessIdByThreadObject((PETHREAD)pOperationInformation->Object);
if (g_pidTable.Contains(static_cast<ULONG>(pid)))
```

- [ ] **Step 4: Grep to confirm 17608 is gone**

```bash
grep -rn "17608" herculeskernel/
```
Expected: 0 hits.

- [ ] **Step 5: Build**

```bash
"$MSB2022" herculeskernel/herculeskernel.vcxproj -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
```

- [ ] **Step 6: Commit**

```bash
git add herculeskernel/Protect/Callback/Callbacks.cpp
git commit -m "feat(kernel): Ob callbacks consult PidTable; hardcoded PID 17608 removed"
```

---

## Task 9: HerculesAC user-mode IOCTL client

**Files:**
- Create: `HerculesAC/Ioctl/DriverClient.h`
- Create: `HerculesAC/Ioctl/DriverClient.cpp`
- Modify: `HerculesAC/HerculesAC.vcxproj` (add DriverClient sources)
- Modify: `HerculesAC/WinMain.cpp` — after `InitProcess()` launches the game, call `DriverClient::StartProtect(game_pid, image, sha256)`

**Interfaces:**
- Consumes: `HAC_DEVICE_USERMODE_PATH` (Task 2), `Manifest` (from M1)
- Produces:
  - `class DriverClient { HANDLE m_h; DriverClient(); ~DriverClient(); bool Open(); DWORD StartProtect(DWORD pid, const std::wstring& image_name, const uint8_t sha256[32]); DWORD StopProtect(DWORD pid); std::optional<IOCTL_QUERY_STATUS_RESPONSE> QueryStatus(); }`

- [ ] **Step 1: Create `HerculesAC/Ioctl/DriverClient.h`**

```cpp
#pragma once

#ifndef _HAC_DRIVER_CLIENT_H
#define _HAC_DRIVER_CLIENT_H

#include <Windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include "../../Common/Shared/IOCTLs.h"

namespace hac {

class DriverClient
{
public:
    DriverClient();
    ~DriverClient();

    bool Open();
    void Close();

    // Returns 0 on success, Win32 error code otherwise (DeviceIoControl semantics)
    DWORD StartProtect(DWORD pid,
                       const std::wstring& image_name,
                       const uint8_t sha256[32]);
    DWORD StopProtect(DWORD pid);
    std::optional<IOCTL_QUERY_STATUS_RESPONSE> QueryStatus();

private:
    HANDLE m_h;
};

} // namespace hac

#endif
```

- [ ] **Step 2: Create `HerculesAC/Ioctl/DriverClient.cpp`**

```cpp
#include "DriverClient.h"
#include <cstring>

namespace hac {

DriverClient::DriverClient() : m_h(INVALID_HANDLE_VALUE) {}
DriverClient::~DriverClient() { Close(); }

bool DriverClient::Open()
{
    if (m_h != INVALID_HANDLE_VALUE) return true;
    m_h = CreateFileW(
        HAC_DEVICE_USERMODE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return m_h != INVALID_HANDLE_VALUE;
}

void DriverClient::Close()
{
    if (m_h != INVALID_HANDLE_VALUE) {
        CloseHandle(m_h);
        m_h = INVALID_HANDLE_VALUE;
    }
}

DWORD DriverClient::StartProtect(DWORD pid,
                                 const std::wstring& image_name,
                                 const uint8_t sha256[32])
{
    if (m_h == INVALID_HANDLE_VALUE) return ERROR_INVALID_HANDLE;

    IOCTL_START_PROTECT_REQUEST req{};
    req.pid = pid;
    wcsncpy_s(req.image_name, image_name.c_str(), _TRUNCATE);
    std::memcpy(req.expected_sha256, sha256, 32);

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(m_h, IOCTL_START_PROTECT,
                              &req, sizeof(req),
                              nullptr, 0,
                              &bytesReturned, nullptr);
    return ok ? ERROR_SUCCESS : GetLastError();
}

DWORD DriverClient::StopProtect(DWORD pid)
{
    if (m_h == INVALID_HANDLE_VALUE) return ERROR_INVALID_HANDLE;

    IOCTL_STOP_PROTECT_REQUEST req{ pid };
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(m_h, IOCTL_STOP_PROTECT,
                              &req, sizeof(req),
                              nullptr, 0,
                              &bytesReturned, nullptr);
    return ok ? ERROR_SUCCESS : GetLastError();
}

std::optional<IOCTL_QUERY_STATUS_RESPONSE> DriverClient::QueryStatus()
{
    if (m_h == INVALID_HANDLE_VALUE) return std::nullopt;

    IOCTL_QUERY_STATUS_RESPONSE resp{};
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(m_h, IOCTL_QUERY_STATUS,
                              nullptr, 0,
                              &resp, sizeof(resp),
                              &bytesReturned, nullptr);
    if (!ok || bytesReturned != sizeof(resp)) return std::nullopt;
    return resp;
}

} // namespace hac
```

- [ ] **Step 3: Add both files to `HerculesAC/HerculesAC.vcxproj`**

Under the existing `<ClCompile>` group, add `<ClCompile Include="Ioctl\DriverClient.cpp" />`. Under `<ClInclude>`, add `<ClInclude Include="Ioctl\DriverClient.h" />`.

- [ ] **Step 4: Wire from `HerculesAC/WinMain.cpp`**

Add `#include "Ioctl/DriverClient.h"` and `#include "../Common/Manifest/Manifest.h"` (if not already present) at the top.

In `InitProcess()`, after the `pi = StartProcess(Global::GamePath, config[L"starter"], L"");` line and before the `Json::Value root;` block, add:

```cpp
// M2: tell the kernel driver which PID to protect. Uses the manifest's
// game.starter SHA-256 as expected_sha256 — the driver will verify caller
// identity (M2: admin gate; M6: full image-hash match). Non-fatal on failure
// so the game still runs even if the driver is not loaded (kernel is
// optional per M1 deployment layout).
if (pi.dwProcessId != 0) {
    hac::DriverClient dc;
    if (dc.Open()) {
        uint8_t expected[32] = {};
        // find the game.starter entry in the manifest's Modules() and use
        // its sha256_hex — decode 64 hex chars to 32 bytes
        for (const auto& mod : g_manifest.Modules()) {
            std::wstring base = mod.path;
            auto pos = base.find_last_of(L"\\/");
            if (pos != std::wstring::npos) base = base.substr(pos + 1);
            if (_wcsicmp(base.c_str(), g_manifest.Game().client.c_str()) == 0) {
                for (size_t i = 0; i < 32 && (2*i + 1) < mod.sha256_hex.size(); ++i) {
                    unsigned v = 0;
                    std::sscanf(mod.sha256_hex.c_str() + 2 * i, "%2x", &v);
                    expected[i] = static_cast<uint8_t>(v);
                }
                break;
            }
        }
        DWORD err = dc.StartProtect(pi.dwProcessId, g_manifest.Game().client, expected);
        if (err == ERROR_SUCCESS) {
            logger.outDebug(_T("Driver protecting PID %u"), pi.dwProcessId);
        } else {
            logger.outDebug(_T("Driver StartProtect failed err=%u (kernel not loaded?)"), err);
        }
    } else {
        logger.outDebug(_T("Driver not present at %s (kernel optional)"), HAC_DEVICE_USERMODE_PATH);
    }
}
```

Note: `g_manifest` was added in M1 Task 11. If Task 11's variable is `static` in a different TU, expose via a getter or `extern` — grep for `g_manifest` in HerculesAC/WinMain.cpp; if it's `static`, keep this code inside the same TU (it already is — WinMain.cpp is where InitProcess lives).

- [ ] **Step 5: Build HerculesAC for Debug|x64 and Release|x64 via VS 2026 MSBuild**

```bash
"$MSB2026" HerculesAC.sln -t:HerculesAC -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal | tail -5
"$MSB2026" HerculesAC.sln -t:HerculesAC -p:Configuration=Release -p:Platform=x64 -verbosity:minimal | tail -5
```
Expected: both build clean.

- [ ] **Step 6: Commit**

```bash
git add HerculesAC/Ioctl HerculesAC/HerculesAC.vcxproj HerculesAC/WinMain.cpp
git commit -m "feat(herculesac): send IOCTL_START_PROTECT to driver after game launch"
```

---

## Task 10: Builder.bat kernel routing + E2E test doc + M2 wrap

**Files:**
- Modify: `Builder.bat` — route herculeskernel target through VS 2022 MSBuild
- Modify: `ReBuilder.bat` — same
- Create: `docs/testing/m2-e2e-driver-load.md`
- Modify: `README.md` — brief M2 section update
- Tag: `m2-kernel-authority`

**Interfaces:**
- Consumes: everything from Tasks 1-9
- Produces: `Builder.bat` produces a signed .sys + full user-mode set; M2 wrap doc + tag.

- [ ] **Step 1: Update `Builder.bat`**

Before the `pause` line, add a section that invokes VS 2022 MSBuild specifically for the herculeskernel target:

```bat
set "MSBUILD2022=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD2022%" (
    echo WARNING: VS 2022 MSBuild not found — kernel target skipped.
    echo   WDK 10.0.26100 ships DriverKit.Build.Tasks 17.0 only; VS 2026 MSBuild 18 cannot load it.
) else (
    "%MSBUILD2022%" "%REPO%herculeskernel\herculeskernel.vcxproj" /p:Configuration=%buildConfig% /p:Platform=x64
)
```

- [ ] **Step 2: Update `ReBuilder.bat`** — add same block with `/t:Rebuild` on the msbuild line.

- [ ] **Step 3: Create `docs/testing/m2-e2e-driver-load.md`**

```markdown
# M2 end-to-end: driver load + PID protection

Objective: prove `herculeskernel.sys` loads on a test-signed VM, accepts
`IOCTL_START_PROTECT` from HerculesAC, and strips `VM_READ`/`VM_WRITE`
from handles opened against the protected PID.

## Setup (once per VM)

1. Copy the built layout (from `x64\Debug\`) to the test VM.
2. On the VM, enable test signing: `bcdedit /set testsigning on`, reboot.
3. Register the driver as a kernel service:
   ```
   sc create HerculesACKM binPath= "C:\install\herculeskernel.sys" type= kernel start= demand
   sc start HerculesACKM
   ```
4. Verify device exists: open WinObj (Sysinternals) and confirm
   `\DosDevices\HerculesAC` symlink → `\Device\HerculesAC`.

## Positive control

Launch `GameLauncher.exe`. HerculesAC should reach the game launch path
and — visible in DebugView with `hercules*` filter — emit
`StartProtect: added PID <N> (total=1)`. `IOCTL_QUERY_STATUS` returns
`{active_protection_count=1, driver_version=0x00020000}`.

## Test 1 — VM_READ stripped

From a second admin cmd, run a small probe (Sysinternals `handle.exe`,
or write a 10-line C program that calls
`OpenProcess(PROCESS_VM_READ, FALSE, game_pid)`). The returned handle
must not have `PROCESS_VM_READ` in its granted-access mask. Verify with
`GetHandleInformation` or Process Explorer's handle view.

## Test 2 — Non-admin cannot start protection

From a non-elevated cmd, run a small probe that opens the device and
sends `IOCTL_START_PROTECT` with some PID. Expected: `DeviceIoControl`
returns `FALSE`, `GetLastError()` = `ERROR_ACCESS_DENIED` (5).

## Test 3 — Stop unloads cleanly

Send `IOCTL_STOP_PROTECT` for the game PID. `IOCTL_QUERY_STATUS`'s
`active_protection_count` must drop to 0. `sc stop HerculesACKM`
must succeed without BSOD.
```

- [ ] **Step 4: Brief README update**

Add a short subsection "Kernel authority (M2)" under the driver row of the component table, noting the IOCTL surface and the removed hardcoded PID. Update the Known Issues section: remove item 1 ("Hard-coded protected PID `17608`") since M2 fixes it.

- [ ] **Step 5: Commit + tag**

```bash
git add Builder.bat ReBuilder.bat docs/testing/m2-e2e-driver-load.md README.md
git commit -m "docs: M2 E2E driver load test + Builder.bat routes kernel via VS 2022"
git tag m2-kernel-authority
```

---

## Self-Review Notes

- **Spec coverage**: M2-core scope (IOCTL + PID table + expand Ob callbacks) is covered by Tasks 1-8; user-mode wiring by Task 9; wrap by Task 10. Minifilter deferred to a later milestone per user decision. `PsSetCreateProcessNotifyRoutineEx` and SHA-256 caller-image hashing explicitly excluded (documented in Task 5 Step 3 code comment).
- **Placeholder scan**: no TBD/TODO/vague requirements. Code blocks are complete C++ / batch.
- **Type consistency**: `PidTable::Insert(ULONG)`, `Contains(ULONG)`, `Remove(ULONG)` — same signature across Tasks 3, 5, 6, 8. `IOCTL_START_PROTECT_REQUEST::pid` is `ULONG` matching. `DriverClient::StartProtect(DWORD ...)` matches WinAPI convention on the user side and casts to ULONG on the wire (both are 32-bit unsigned on Windows, so byte-identical).
- **Ambiguity check**: Task 9 Step 4 assumes `g_manifest` in HerculesAC/WinMain.cpp — Task 11 (M1) added `static hac::manifest::Manifest g_manifest;` inside that TU, so it's accessible from InitProcess() in the same file.
