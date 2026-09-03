#include "../Driver.h"
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
