#include "ListOp.h"


VOID
FASTCALL
LopInsertHeadSync(
    _Inout_ PLIST_T      List,  
    _Inout_ PLIST_ENTRY  Elem 
)
{
    PLIST_ENTRY head  = NULL;
    PKSPIN_LOCK sLock = NULL;
    KIRQL       irql  = PASSIVE_LEVEL;

    ASSERT(List != NULL);
    ASSERT(Elem != NULL);
    
    sLock = List->SpinLock;
    KeAcquireSpinLock(sLock, &irql);
    {
        head = &List->Head;

        InsertHeadList(head, Elem);
    }
    KeReleaseSpinLock(sLock, irql);

    return;
}

