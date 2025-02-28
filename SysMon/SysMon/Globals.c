#include <ntddk.h>
#include "..\SysMon\Globals.h"
#include "..\SysMon\SysMonPublic.h"

void Globals_Init(PGlobals globals, ULONG maxItems) {
	InitializeListHead(&globals->m_ItemsHead);
	ExInitializeFastMutex(&globals->m_Lock);
	globals->m_Count = 0; 
	globals->m_MaxCount = maxItems; 
    RtlZeroMemory(&globals->m_RegCookie, sizeof(LARGE_INTEGER));
}

BOOLEAN Globals_AddItem(PGlobals globals, LIST_ENTRY* entry) {
    ExAcquireFastMutex(&globals->m_Lock);

    // Check if the list has reached its maximum size
    if (globals->m_Count == globals->m_MaxCount) {
        PLIST_ENTRY head = RemoveHeadList(&globals->m_ItemsHead);
        if (head != &globals->m_ItemsHead) {  // Ensure the list is not empty
            // Retrieve the item from the list entry
            PFullItem item = CONTAINING_RECORD(head, FullItem, Entry);

            // Free the item safely
            ExFreePool(item);
            globals->m_Count--;  // Decrement the count
        }
    }

    // Before inserting, ensure the item is not already in the list
    PLIST_ENTRY current = globals->m_ItemsHead.Flink;
    while (current != &globals->m_ItemsHead) {
        PFullItem item = CONTAINING_RECORD(current, FullItem, Entry);
        if (item == (PFullItem)entry) {  // Prevent double insertion
            ExReleaseFastMutex(&globals->m_Lock);
            return FALSE;  // Item already in the list
        }
        current = current->Flink;
    }

    // Insert the new item into the list
    InsertTailList(&globals->m_ItemsHead, entry);
    globals->m_Count++;  // Increment the count

    ExReleaseFastMutex(&globals->m_Lock);
    return TRUE;  // Success
}

LIST_ENTRY* Globals_RemoveItem(PGlobals globals) {
    ExAcquireFastMutex(&globals->m_Lock);
    PLIST_ENTRY item = RemoveHeadList(&globals->m_ItemsHead);
    if (item == &globals->m_ItemsHead) { // If the list is empty
        ExReleaseFastMutex(&globals->m_Lock);
        return NULL;  // No items to remove
    }

    globals->m_Count--;  // Decrement the count

    ExReleaseFastMutex(&globals->m_Lock);
    return item;  // Return the item
}

void Globals_AddHeadItem(PGlobals globals, LIST_ENTRY* entry) {
    ExAcquireFastMutex(&globals->m_Lock);

    // Check if the item is already in the list
    PLIST_ENTRY current = globals->m_ItemsHead.Flink;
    while (current != &globals->m_ItemsHead) {
        PFullItem item = CONTAINING_RECORD(current, FullItem, Entry);
        if (item == (PFullItem)entry) {  // Prevent double insertion
            ExReleaseFastMutex(&globals->m_Lock);
            return;
        }
        current = current->Flink;
    }

    // Insert the item at the head of the list
    InsertHeadList(&globals->m_ItemsHead, entry);
    globals->m_Count++;  // Increment the count

    ExReleaseFastMutex(&globals->m_Lock);
}
