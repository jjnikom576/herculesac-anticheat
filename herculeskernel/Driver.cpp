#include "Driver.h"
#include "Protect/Callback/Callbacks.h"

VOID InitProtect(IN PDRIVER_OBJECT pDriver_Object)
{
    SetThreadCallbacks(pDriver_Object);
    SetProcessCallbacks(pDriver_Object);
}


VOID UninstallProtect()
{
    UnThreadCallbacks();
    UnProcessCallbacks();
}


EXTERN_C
VOID Unload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
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
    InitProtect(DriverObject);
    return status;
}