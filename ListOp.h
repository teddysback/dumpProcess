#pragma once

#include "WdmDriver.h"


typedef struct _LIST_T
{
    LIST_ENTRY  Head;           // List Head entry
    PKSPIN_LOCK SpinLock;       // Pointer to a Lock for sync op

}LIST_T, *PLIST_T;


//
// Calls init on list head and list spin lock
//
FORCEINLINE
VOID
LopInit(
    _Inout_ PLIST_T List
)
{
    ASSERT(List != NULL);

    InitializeListHead(&List->Head);
    KeInitializeSpinLock(List->SpinLock);

    return;
}

// 
// Syncronised insert of Elem (PROCESS_T->ListEntry) in List after the head sentinel (List->Head), using spin lock (List->SpinLock)
//
VOID
FASTCALL
LopInsertHeadSync(
    _Inout_ PLIST_T      List,
    _Inout_ PLIST_ENTRY  Elem
);


/* List traverse */

PLIST_ENTRY
FORCEINLINE
LopListBegin(
    _In_ PLIST_T List
)
{
    ASSERT(List != NULL);

    return List->Head.Flink;
}

PLIST_ENTRY
FORCEINLINE
LopListEnd(
    _In_ PLIST_T List
)
{
    ASSERT(List != NULL);

    return List->Head.Blink;
}

PLIST_ENTRY
FORCEINLINE
LopEntryNext(
    _In_ PLIST_ENTRY Elem
)
{
    ASSERT(Elem != NULL);

    return Elem->Flink;
}

BOOLEAN
FORCEINLINE
LopIsListEmpty(
    _In_ PLIST_T List
)
{
    ASSERT(List != NULL);

    return (BOOLEAN)(List->Head.Flink == &List->Head);
}