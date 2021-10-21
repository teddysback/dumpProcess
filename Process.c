#include "Process.h"


PPROCESS_T
PrcAlloc(
    _In_   HANDLE  ParentId,
    _In_   HANDLE  ProcessId,
    _In_   BOOLEAN Create
)
{
    PPROCESS_T p = NULL;

    p = (PPROCESS_T)ExAllocatePoolWithTag(NonPagedPool, sizeof(PROCESS_T), IOC_TAG_NAME);
    if (p == NULL)
    {
        goto cleanup;
    }
    RtlZeroMemory(p, sizeof(PROCESS_T));

    p->Info.ParentId = ParentId;
    p->Info.ProcessId = ProcessId;
    p->Info.Create = Create;


cleanup:

    return p;
}


VOID
PrcFree(
    _Inout_ PPROCESS_T Process
)
{
    if (Process != NULL)
    {
        if (Process->Mdl != NULL)
        {
            MmUnlockPages(Process->Mdl);   
            IoFreeMdl(Process->Mdl);
            Process->Mdl = NULL;
        }

        ExFreePoolWithTag(Process, IOC_TAG_NAME);
        Process = NULL;
    }

    return;
}


VOID
PrcInsertProcess(
    _Inout_ PLIST_T     List,
    _Inout_ PPROCESS_T  Process
)
{
    ASSERT(List != NULL);
    ASSERT(Process != NULL);

    LopInsertHeadSync(List, &(Process->ListEntry));

    return;
}


PPROCESS_T
PrcRemoveProcessId(
    _Inout_  PLIST_T  List,
    _In_opt_ PHANDLE  ParentId,
    _In_     PHANDLE  ProcessId
)
{
    PPROCESS_T   p      = NULL;
    PKSPIN_LOCK  sLock  = NULL;
    KIRQL        irql   = PASSIVE_LEVEL;
    
    ASSERT(List != NULL);
    ASSERT(ProcessId != NULL);

    p = PrcFindProcessId(List, *ProcessId);
    if (p != NULL)
    {
        sLock = List->SpinLock;

        KeAcquireSpinLock(sLock, &irql);
        
        if (ParentId != NULL)
        {
            ASSERT(p->Info.ParentId == *ParentId);
        }
        RemoveEntryList(&p->ListEntry); 
        
        KeReleaseSpinLock(sLock, irql);
    }

    return p;
}


PPROCESS_T
PrcFindProcessId(
    _In_ PLIST_T List,
    _In_ HANDLE  ProcessId
)
{
    PPROCESS_T   p      = NULL;
    PKSPIN_LOCK  sLock  = NULL;
    KIRQL        irql   = PASSIVE_LEVEL;

    ASSERT(List != NULL);


    sLock = List->SpinLock;
    KeAcquireSpinLock(sLock, &irql);
    {
        PLIST_ENTRY  head = &List->Head;
        PLIST_ENTRY  e = NULL;

        for (e = LopListBegin(List); e != head; e = LopEntryNext(e), p = NULL)
        {
            p = CONTAINING_RECORD(e, PROCESS_T, ListEntry);

            // Found PID in list
            if (p->Info.ProcessId == ProcessId)
            {
                break;
            }
            p = NULL; // i know i already did this in for(;;p=null); vizibility
        }
    }
    KeReleaseSpinLock(sLock, irql);

    return p;
}


VOID
PrcUnlockMdlsFromList(
    _Inout_ PLIST_T List
)
{
    PPROCESS_T   p = NULL;
    PKSPIN_LOCK  sLock = NULL;
    KIRQL        irql = PASSIVE_LEVEL;

    ASSERT(List != NULL);


    sLock = List->SpinLock;
    KeAcquireSpinLock(sLock, &irql);
    {
        PLIST_T      listT = List;
        PLIST_ENTRY  e = LopListBegin(listT);
        PLIST_ENTRY  next = NULL;

        while (!IsListEmpty(&listT->Head))
        {
            next = e->Flink;

            p = CONTAINING_RECORD(e, PROCESS_T, ListEntry);
            if (p->Mdl != NULL)
            {
                MmUnlockPages(p->Mdl);
                IoFreeMdl(p->Mdl);
                p->Mdl = NULL;
            }

            PrcFree(p);
            p = NULL;
            e = next;
        }
    }
    KeReleaseSpinLock(sLock, irql);

    return;
}


VOID
PrcFreeList(
    _Inout_ PLIST_T List
)
{
    PPROCESS_T   p      = NULL;
    PKSPIN_LOCK  sLock  = NULL;
    KIRQL        irql   = PASSIVE_LEVEL;

    ASSERT(List != NULL);


    sLock = List->SpinLock;
    KeAcquireSpinLock(sLock, &irql);
    {
        PLIST_T      listT = List;
        PLIST_ENTRY  e = LopListBegin(listT);
        PLIST_ENTRY  next = NULL;
            
        while (!IsListEmpty(&listT->Head))
        {
            next = e->Flink;
            RemoveEntryList(e);
            
            p = CONTAINING_RECORD(e, PROCESS_T, ListEntry);

            PrcFree(p);
            p = NULL;
            e = next;
        }
    }
    KeReleaseSpinLock(sLock, irql);

    return;
}


PPROCESS_T
PrcRemoveHeadProcess(
    _Inout_ PLIST_T List
)
{
    PPROCESS_T   p      = NULL;
    PKSPIN_LOCK  sLock  = NULL;
    KIRQL        irql   = PASSIVE_LEVEL;

    ASSERT(List != NULL);


    sLock = List->SpinLock;
    KeAcquireSpinLock(sLock, &irql);
    {
        PLIST_ENTRY  e = NULL;

        e = LopListBegin(List);
        RemoveEntryList(e);
        
        p = CONTAINING_RECORD(e, PROCESS_T, ListEntry);
    }
    KeReleaseSpinLock(sLock, irql);

    return p;
}


VOID
PrcUnlockMdlList(
    _Inout_ PLIST_T List
)
{
    PPROCESS_T   p      = NULL;
    PKSPIN_LOCK  sLock  = NULL;
    KIRQL        irql   = PASSIVE_LEVEL;

    ASSERT(List != NULL);


    sLock = List->SpinLock;
    KeAcquireSpinLock(sLock, &irql);
    {
        PLIST_ENTRY  head = &List->Head;
        PLIST_ENTRY  e = NULL;

        for (e = LopListBegin(List); e != head; e = LopEntryNext(e), p = NULL)
        {
            p = CONTAINING_RECORD(e, PROCESS_T, ListEntry);
            if (p->Mdl != NULL)
            {
                MmUnlockPages(p->Mdl);
                IoFreeMdl(p->Mdl);
                p->Mdl = NULL;
            }
        }
    }
    KeReleaseSpinLock(sLock, irql);

    return;
}