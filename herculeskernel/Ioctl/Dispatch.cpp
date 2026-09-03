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
    case IOCTL_START_PROTECT: {
        LUID debugPriv = RtlConvertLongToLuid(SE_DEBUG_PRIVILEGE);
        if (!SeSinglePrivilegeCheck(debugPriv, UserMode)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(IOCTL_START_PROTECT_REQUEST)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        PIOCTL_START_PROTECT_REQUEST req =
            static_cast<PIOCTL_START_PROTECT_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
        status = g_pidTable.Insert(req->pid);
        break;
    }
    case IOCTL_STOP_PROTECT: {
        LUID debugPriv = RtlConvertLongToLuid(SE_DEBUG_PRIVILEGE);
        if (!SeSinglePrivilegeCheck(debugPriv, UserMode)) {
            status = STATUS_ACCESS_DENIED;
            break;
        }
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(IOCTL_STOP_PROTECT_REQUEST)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        PIOCTL_STOP_PROTECT_REQUEST req =
            static_cast<PIOCTL_STOP_PROTECT_REQUEST>(Irp->AssociatedIrp.SystemBuffer);
        status = g_pidTable.Remove(req->pid);
        break;
    }
    case IOCTL_QUERY_STATUS: {
        if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(IOCTL_QUERY_STATUS_RESPONSE)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        PIOCTL_QUERY_STATUS_RESPONSE resp =
            static_cast<PIOCTL_QUERY_STATUS_RESPONSE>(Irp->AssociatedIrp.SystemBuffer);
        resp->active_protection_count = g_pidTable.Count();
        resp->driver_version          = 0x00020000;
        info   = sizeof(IOCTL_QUERY_STATUS_RESPONSE);
        status = STATUS_SUCCESS;
        break;
    }
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
