#include "Driver.h"
#include "Protect/Callback/Callbacks.h"
#include "Ioctl/Dispatch.h"

VOID InitProtect(IN PDRIVER_OBJECT pDriver_Object)
{
    SetThreadCallbacks(pDriver_Object);
    SetProcessCallbacks(pDriver_Object);
    InitThreadNotify();
}

VOID UninstallProtect()
{
    UninstallThreadNotify();
    UnThreadCallbacks();
    UnProcessCallbacks();
}


EXTERN_C
VOID Unload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    hac::ioctl::DeleteDevice();
    UninstallProtect();
    outLog("Driver Unload!!!");
}

EXTERN_C
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    DriverObject->DriverUnload = Unload;
    NTSTATUS status = STATUS_SUCCESS;
    outLog("Driver Loaded!!!");
    status = hac::ioctl::CreateDevice(DriverObject);
    if (!NT_SUCCESS(status)) {
        outLog("CreateDevice failed 0x%08X", status);
        return status;
    }
    InitProtect(DriverObject);
    return status;
}