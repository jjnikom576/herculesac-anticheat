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
