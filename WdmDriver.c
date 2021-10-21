/*++

Module Name:

    WdmDriver.c

Abstract:

    This is the main module of the WdmDriver miniFilter driver.

Environment:

    Kernel mode

--*/

#include "WdmDriver.h"
#include "Public.h"
#include "ListOp.h"
#include "Process.h"

#include "Trace.h"
#include "WdmDriver.tmh"


typedef struct _IOC_DRIVER
{
    LIST_T      ProcessList;                 // A list of active processes (used internal for print)
    LIST_T      ProcessQueue;                // Processes queue for UM

    KEVENT      EventProcessCreateClose;     // A proc has been CREATED / CLOSED  (for proc queue)
    KEVENT      EventDriverUnload;           // Driver Unload has been called
    HANDLE      ThreadHandle;
    
    PIRP        IrpCurrent;                  // Current pended IRP 
    KSPIN_LOCK  IrpLock;                     // SpinLock to use IRP

} IOC_DRIVER, *PIOC_DRIVER;

// 
//  Structure that contains all the global data structures
//  used throughout the filter.
//
IOC_DRIVER gDriver;

KSTART_ROUTINE ProcessIoctlNotifyRoutine;

NTSTATUS
ProcessIoctlDumpRoutine(
    _In_ PIRP Irp
);

VOID
CancelIrpRoutine(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
);

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
    represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
    driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS            status          = STATUS_UNSUCCESSFUL;
    UNICODE_STRING      symLinkName     = { 0 };
    UNICODE_STRING      devName         = { 0 };
    PDEVICE_OBJECT      deviceObject    = NULL;
    OBJECT_ATTRIBUTES   objAtr          = { 0 };

    UNREFERENCED_PARAMETER(RegistryPath);

    WPP_INIT_TRACING(DriverObject, RegistryPath);

    LogInfo("\"DriverEntry\" called.");

    __try
    {
        // init.. 
        RtlZeroMemory(&gDriver, sizeof(gDriver));
        KeInitializeSpinLock(&gDriver.IrpLock);

        // init km proc list 
        gDriver.ProcessList.SpinLock = (PKSPIN_LOCK)ExAllocatePoolWithTag(NonPagedPool, sizeof(KSPIN_LOCK), IOC_TAG_NAME);
        if (gDriver.ProcessList.SpinLock == NULL)
        {
            LogErrorHex("ExAllocatePoolWithTag", 0);
            __leave;
        }
        LopInit(&gDriver.ProcessList);
        
        // init um proc queue
        gDriver.ProcessQueue.SpinLock = (PKSPIN_LOCK)ExAllocatePoolWithTag(NonPagedPool, sizeof(KSPIN_LOCK), IOC_TAG_NAME);
        if (gDriver.ProcessQueue.SpinLock == NULL)
        {
            LogErrorHex("ExAllocatePoolWithTag", 0);
            __leave;
        }
        LopInit(&gDriver.ProcessQueue);

        // init events
        KeInitializeEvent(&gDriver.EventProcessCreateClose, SynchronizationEvent, FALSE);
        KeInitializeEvent(&gDriver.EventDriverUnload, SynchronizationEvent, FALSE);
        
        // Device name
        RtlInitUnicodeString(&devName, IOC_DEVICE_NAME);
        status = IoCreateDevice(
            DriverObject,               // Our Driver Object
            0,                          // We don't use a device extension
            &devName,                   // Device name
            FILE_DEVICE_UNKNOWN,        // Device type
            FILE_DEVICE_SECURE_OPEN,    // Device characteristics
            FALSE,                      // Not an exclusive device
            &deviceObject);             // Returned ptr to Device Object
        if (!NT_SUCCESS(status))
        {
            LogErrorNt("IoCreateDevice", status);
            __leave;
        }

        DriverObject->MajorFunction[IRP_MJ_CREATE] = IocDispatchCreateClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = IocDispatchCreateClose;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IocDispatchDeviceControl;
        DriverObject->DriverUnload = DriverUnload;

        // Symbolic link
        RtlInitUnicodeString(&symLinkName, IOC_SYMBOLIC_LINK_NAME);
        status = IoCreateSymbolicLink(&symLinkName, &devName);
        if (!NT_SUCCESS(status))
        {
            LogErrorNt("IoCreateSymbolicLink", status);
            __leave;
        }

        // create thread
        InitializeObjectAttributes(&objAtr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

        status = PsCreateSystemThread(
            &gDriver.ThreadHandle,
            (ACCESS_MASK)SYNCHRONIZE,   
            &objAtr,
            (HANDLE)0,
            NULL,
            ProcessIoctlNotifyRoutine,
            NULL);              
        if (!NT_SUCCESS(status))
        {
            LogErrorNt("PsCreateSystemThread", status);
            __leave;
        }

        // Hook process create
        status = PsSetCreateProcessNotifyRoutine(
            (PCREATE_PROCESS_NOTIFY_ROUTINE)CreateProcessNotifyRoutine, 
            FALSE);
        if (!NT_SUCCESS(status))
        {
            LogErrorNt("PsSetCreateProcessNotifyRoutine", status);
            __leave;
        }

        status = STATUS_SUCCESS;
    }
    __finally
    {
        if (!NT_SUCCESS(status))
        {
            PsSetCreateProcessNotifyRoutine((PCREATE_PROCESS_NOTIFY_ROUTINE)CreateProcessNotifyRoutine, TRUE);
            ZwClose(gDriver.ThreadHandle);
            IoDeleteSymbolicLink(&symLinkName);
            IoDeleteDevice(deviceObject);

            if (gDriver.ProcessList.SpinLock != NULL)
            { 
                ExFreePoolWithTag(gDriver.ProcessList.SpinLock, IOC_TAG_NAME);
                gDriver.ProcessList.SpinLock = NULL;
            }
            if (gDriver.ProcessQueue.SpinLock != NULL)
            {
                ExFreePoolWithTag(gDriver.ProcessQueue.SpinLock, IOC_TAG_NAME);
                gDriver.ProcessQueue.SpinLock = NULL;
            }

            WPP_CLEANUP(DriverObject);
        }
    }

    return status;
}


VOID
DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    NTSTATUS        status      = STATUS_UNSUCCESSFUL;
    UNICODE_STRING  symLinkName = { 0 };

    
    LogInfo("\"DriverUnload\" called");

    // Unhook process create
    status = PsSetCreateProcessNotifyRoutine(
        (PCREATE_PROCESS_NOTIFY_ROUTINE)CreateProcessNotifyRoutine,
        TRUE);
    if (!NT_SUCCESS(status))
    {
        LogErrorNt("PsSetCreateProcessNotifyRoutine", status);
    }

    // Delete symbolic link and device 
    RtlInitUnicodeString(&symLinkName, IOC_SYMBOLIC_LINK_NAME);

    status = IoDeleteSymbolicLink(&symLinkName);
    if (!NT_SUCCESS(status))
    {
        LogErrorNt("IoDeleteSymbolicLink", status);
    }

    if (DriverObject->DeviceObject != NULL)
    {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    // signal driver unload
    KeSetEvent(&gDriver.EventDriverUnload, IO_NO_INCREMENT, FALSE);

    // wait
    status = ZwWaitForSingleObject(gDriver.ThreadHandle, FALSE, NULL);  
    ZwClose(gDriver.ThreadHandle);

    // Free procs from lists & free list_t
    if (gDriver.ProcessList.SpinLock != NULL)
    {
        PrcFreeList(&gDriver.ProcessList);

        ExFreePoolWithTag(gDriver.ProcessList.SpinLock, IOC_TAG_NAME);
        gDriver.ProcessList.SpinLock = NULL;
    }
    
    if (gDriver.ProcessQueue.SpinLock != NULL)
    {
        PrcFreeList(&gDriver.ProcessQueue);

        ExFreePoolWithTag(gDriver.ProcessQueue.SpinLock, IOC_TAG_NAME);
        gDriver.ProcessQueue.SpinLock = NULL;
    }


    WPP_CLEANUP(DriverObject);

    return;
}


_Use_decl_annotations_
NTSTATUS
IocDispatchCreateClose(
    _Inout_ struct _DEVICE_OBJECT *DeviceObject,
    _Inout_ struct _IRP           *Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}


_Use_decl_annotations_
NTSTATUS 
IocDispatchDeviceControl(
    _Inout_ struct _DEVICE_OBJECT *DeviceObject,
    _Inout_ struct _IRP           *Irp
)
{
    NTSTATUS            irpStatus       = STATUS_SUCCESS;
    PIO_STACK_LOCATION  irpSp           = NULL;

    UNREFERENCED_PARAMETER(DeviceObject);


    irpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_NOTIFY_CALLBACK:
        {
            KIRQL irql = PASSIVE_LEVEL;

            while (gDriver.IrpCurrent != NULL) {} 
            
            KeAcquireSpinLock(&gDriver.IrpLock, &irql);
            {
                gDriver.IrpCurrent = Irp;

                IoMarkIrpPending(Irp);
                irpStatus = STATUS_PENDING;

                IoSetCancelRoutine(Irp, CancelIrpRoutine);
            }
            KeReleaseSpinLock(&gDriver.IrpLock, irql);

            // Will mark completion of IRP in ProcessIoctlNotifyRoutine
            
            break;
        }
        case IOCTL_DUMP_PROCESS:
        {
            irpStatus = ProcessIoctlDumpRoutine(Irp);

            // Will mark completion of IRP in ProcessIoctlDumpRoutine

            break;
        }
        case IOCTL_EXIT:
        {
            // unlock MDLs
            PrcUnlockMdlList(&gDriver.ProcessList);

            // Fill completion status
            Irp->IoStatus.Information = 0;
            Irp->IoStatus.Status = irpStatus;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            break;
        }
        default:
        {
            // Fill completion status
            irpStatus = STATUS_INVALID_DEVICE_REQUEST;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            break;
        }
    }
    
    return irpStatus;
}


VOID
CreateProcessNotifyRoutine(
    _In_ HANDLE  ParentId,
    _In_ HANDLE  ProcessId,
    _In_ BOOLEAN Create
)
{
    PPROCESS_T process = NULL;

    LogInfo("PPID:%p PID:%p %s", ParentId, ProcessId, Create ? "Create" : "Close");
    
    __try
    {
        //
        //  NOTIFY proc queue
        //
        process = PrcAlloc(ParentId, ProcessId, Create);
        if (process != NULL)
        {
            PrcInsertProcess(&gDriver.ProcessQueue, process); 

            KeSetEvent(&gDriver.EventProcessCreateClose, IO_NO_INCREMENT, FALSE);
        }

        //
        //  Internal proc list
        //
        if (Create)
        {
            process = PrcAlloc(ParentId, ProcessId, Create); 
            if (process != NULL)
            {
                PrcInsertProcess(&gDriver.ProcessList, process);
                process = NULL;
            }
        }
        else
        {
            process = PrcRemoveProcessId(&gDriver.ProcessList, &ParentId, &ProcessId);
            if (process != NULL)
            {
                PrcFree(process);
                process = NULL;
            }
        }
    }
    __finally
    {
    }

    return;
}



VOID 
ProcessIoctlNotifyRoutine(
    PVOID StartContext
)
/*++

Routine Description:

    Process all the PENDING IRPs.

--*/
{
    NTSTATUS    status          = STATUS_UNSUCCESSFUL;
    PVOID       waitEvents[2]   = { 0 };
    PIRP                irp = NULL;
    PIO_STACK_LOCATION  irpSp = NULL;
    PWCHAR              inBuffer = NULL;
    PWCHAR              outBuffer = NULL;
    ULONG               inBufferLen = 0;
    ULONG               outBufferLen = 0;
    KIRQL               irql = PASSIVE_LEVEL;
    PPROCESS_T p = NULL;


    UNREFERENCED_PARAMETER(StartContext);
    
    for (;;)
    {
        waitEvents[0] = &gDriver.EventProcessCreateClose;
        waitEvents[1] = &gDriver.EventDriverUnload;

        status = KeWaitForMultipleObjects(
            2,
            waitEvents,
            WaitAny,
            Executive,
            KernelMode,
            FALSE,
            NULL,        
            NULL);
        if (status == STATUS_WAIT_0)
        {
            LogInfo("EventProcessCreate || EventProcessClose");

            while (gDriver.IrpCurrent == NULL && KeReadStateEvent(&gDriver.EventDriverUnload) == 0) {} 
            if (KeReadStateEvent(&gDriver.EventDriverUnload) != 0)
            {
                //was signaled
                break;
            }
            
            KeAcquireSpinLock(&gDriver.IrpLock, &irql);
            {
                irp = gDriver.IrpCurrent;
            }
            KeReleaseSpinLock(&gDriver.IrpLock, irql);

            irpSp = IoGetCurrentIrpStackLocation(irp);

            inBufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
            outBufferLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

            inBuffer = (PWCHAR)irp->AssociatedIrp.SystemBuffer;
            outBuffer = (PWCHAR)irp->AssociatedIrp.SystemBuffer;

            p = PrcRemoveHeadProcess(&gDriver.ProcessQueue);
                
            ASSERT(outBufferLen == sizeof(p->Info));
            RtlCopyMemory(outBuffer, &p->Info, sizeof(p->Info));
            PrcFree(p);

            // clean cancel stuff
            IoSetCancelRoutine(irp, NULL);

            // Fill completion status
            irp->IoStatus.Information = sizeof(p->Info);
            irp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(irp, IO_NO_INCREMENT);


            irql = PASSIVE_LEVEL;
            KeAcquireSpinLock(&gDriver.IrpLock, &irql);
            {
                gDriver.IrpCurrent = NULL;
            }
            KeReleaseSpinLock(&gDriver.IrpLock, irql);
                
        }
        else if (status == STATUS_WAIT_1)
        {
            LogInfo("EventDriverUnload");

            break;
        }
        else
        {
            LogErrorNt("KeWaitForMultipleObjects failed", status);
            continue; // continue processing
        }
    }


    return;
}



VOID 
CancelIrpRoutine(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP           Irp
)
{
    PIRP cancelIrp = NULL;
    KIRQL irql = PASSIVE_LEVEL;

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    UNREFERENCED_PARAMETER(DeviceObject);

    KeAcquireSpinLock(&gDriver.IrpLock, &irql);
    {
        cancelIrp = gDriver.IrpCurrent;
        gDriver.IrpCurrent = NULL;
    }
    KeReleaseSpinLock(&gDriver.IrpLock, irql);

    if (cancelIrp->Cancel)
    {
        cancelIrp->IoStatus.Information = 0;
        cancelIrp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(cancelIrp, IO_NO_INCREMENT);
    }

    return;
}


NTSTATUS
ProcessIoctlDumpRoutine(
    _In_ PIRP Irp
)
{
    PIO_STACK_LOCATION irpSp = NULL;
    PWCHAR inBuffer  = NULL;
    PWCHAR outBuffer = NULL;
    ULONG inBufferLen = 0;
    ULONG outBufferLen = 0;
    PMDL mdl = NULL;
    
    NTSTATUS irpStatus = STATUS_SUCCESS;
    ULONG info = 0;

    PVOID systemBuffer = NULL;

    
    irpSp = IoGetCurrentIrpStackLocation(Irp);

    inBuffer = irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    outBuffer = Irp->UserBuffer;

    inBufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufferLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    //
    // MDL stuff
    //
    {
        __try
        {
            ProbeForRead(inBuffer, inBufferLen, sizeof(WCHAR));
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            irpStatus = GetExceptionCode();
            goto clean_up;
        }

        mdl = IoAllocateMdl(inBuffer, inBufferLen, FALSE, FALSE, NULL);
        if (mdl == NULL)
        {

            irpStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto clean_up;
        }

        __try
        {
            MmProbeAndLockPages(mdl, UserMode, IoReadAccess); 
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            IoFreeMdl(mdl);
            mdl = NULL;

            irpStatus = GetExceptionCode();            
            goto clean_up;
        }

        systemBuffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);
        if (systemBuffer == NULL)
        {
            MmUnlockPages(mdl);
            IoFreeMdl(mdl);
            mdl = NULL;

            irpStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto clean_up;
        }
    }
    
    //
    // check PROCESS_T list for PID
    //
    {
        PPROCESS_T p = NULL;
        ULONG pid = 0;
        PWCHAR endPrt = NULL;


        pid = wcstoul(inBuffer, &endPrt, 10);
        LogInfo(">>> %u", pid);

        p = PrcFindProcessId(&gDriver.ProcessList, (HANDLE)pid);
        if (p == NULL)
        {
            MmUnlockPages(mdl);
            IoFreeMdl(mdl);
            mdl = NULL;

            //goto clean_up;
        }
        else
        {
            p->Mdl = mdl; // sync?
            mdl = NULL;
        }
    }

    //
    // can we write ?
    //
    {
        __try
        {
            ProbeForWrite(outBuffer, outBufferLen, sizeof(WCHAR));
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        { 
            irpStatus = GetExceptionCode();
            goto clean_up;
        }

    }

clean_up:
    Irp->IoStatus.Information = info; 
    Irp->IoStatus.Status = irpStatus; 
    IoCompleteRequest(Irp, IO_NO_INCREMENT);


    return irpStatus;
}