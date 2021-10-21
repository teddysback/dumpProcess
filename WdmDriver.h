#pragma once

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntstrsafe.h>
#include <wchar.h>
#include <Ntifs.h>


#define IOC_TAG_NAME            'COI:'

DRIVER_INITIALIZE   DriverEntry;
DRIVER_UNLOAD       DriverUnload;

_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH     IocDispatchCreateClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH     IocDispatchDeviceControl;


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
);

VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
);

NTSTATUS
IocDispatchCreateClose(
    _Inout_ struct _DEVICE_OBJECT *DeviceObject,
    _Inout_ struct _IRP           *Irp
);

NTSTATUS
IocDispatchDeviceControl(
    _Inout_ struct _DEVICE_OBJECT *DeviceObject,
    _Inout_ struct _IRP           *Irp
);

VOID
CreateProcessNotifyRoutine(
    _In_ HANDLE  ParentId,
    _In_ HANDLE  ProcessId,
    _In_ BOOLEAN Create
);
