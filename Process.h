#pragma once

#include "WdmDriver.h"
#include "Public.h"
#include "ListOp.h"


typedef struct _PROCESS_T
{
    PROC_INFO  Info;           // PPID, PID and Create
    PMDL       Mdl;            // Memory descriptor list
    PVOID      SystemVA;       // System Address Space
    LIST_ENTRY ListEntry;

}PROCESS_T, *PPROCESS_T;



PPROCESS_T
PrcAlloc(
    _In_   HANDLE      ParentId,
    _In_   HANDLE      ProcessId,
    _In_   BOOLEAN     Create
);

VOID
PrcFree(
    _Inout_ PPROCESS_T Process
);

/* Synq */

VOID
PrcInsertProcess(
    _Inout_ PLIST_T     List,
    _Inout_ PPROCESS_T  Process
);

//
// Removes PID from PROCESS_T list
//
// return:
//      - NULL - not found (or head?)
//      - valid pointer to PROCESS_T that was removed from list (free pointer)
PPROCESS_T
PrcRemoveProcessId(
    _Inout_  PLIST_T  List,
    _In_opt_ PHANDLE  ParentId,
    _In_     PHANDLE  ProcessId
);

//
// Find Pid in PROCESS_T list
//
// returns:
//      - NULL - not found
//      - valid ptr
PPROCESS_T
PrcFindProcessId(
    _In_ PLIST_T List,
    _In_ HANDLE  ProcessId
);

VOID
PrcFreeList(
    _Inout_ PLIST_T List
);

PPROCESS_T
PrcRemoveHeadProcess(
    _Inout_ PLIST_T List
);

VOID
PrcUnlockMdlList(
    _Inout_ PLIST_T List
);