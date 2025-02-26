#include <ntddk.h>
#include "..\RemoteThreadDetector\Globals.h"
#include "..\RemoteThreadDetector\RThreadDetPublic.h"

void Globals_Init(PGlobals globals, ULONG maxItems);
BOOLEAN Globals_AddItem(PGlobals globals, LIST_ENTRY* entry);
BOOLEAN Globals_AddHeadItem(PGlobals globals, LIST_ENTRY* entry);
LIST_ENTRY* Globals_RemoveItem(PGlobals globals);
ULONG Globals_AddNewProcess(PGlobals globals, ULONG pid);
ULONG checkPresenceInNewProcessList(PGlobals globals, ULONG pid);
void RemoveProcessFromList(PGlobals globals, ULONG position); //position = where the pid is in the array + 1

void Globals_Init(PGlobals globals, ULONG maxItems) {
	InitializeListHead(&globals->m_ItemsHead);
	ExInitializeFastMutex(&globals->m_Lock);
	globals->m_count = 0; 
	globals->m_MaxCount = maxItems; 
}

BOOLEAN Globals_AddItem(PGlobals globals, LIST_ENTRY* entry) {
	ExAcquireFastMutex(&globals->m_Lock);
	if (globals->m_count == globals->m_MaxCount) {
		PLIST_ENTRY head = RemoveHeadList(&globals->m_ItemsHead);
		if (head != &globals->m_ItemsHead) {//ensure the list is not empty
			RemoteThreadItem* item = CONTAINING_RECORD(head, RemoteThreadItem, Entry);
			ExFreePool(item);
			globals->m_count--;
		}
	}
	PLIST_ENTRY current = globals->m_ItemsHead.Flink; 
	while (current != &globals->m_ItemsHead) {
		if (current == entry) {// to prevent double insertion
			ExReleaseFastMutex(&globals->m_Lock);
			return FALSE; 
		}
		current = current->Flink;
	}
	InsertTailList(&globals->m_ItemsHead, entry);
	globals->m_count++;
	ExReleaseFastMutex(&globals->m_Lock);
	return TRUE; 
}

BOOLEAN Globals_AddHeadItem(PGlobals globals, LIST_ENTRY* entry) {
	ExAcquireFastMutex(&globals->m_Lock);
	if (globals->m_count == globals->m_MaxCount) {
		PLIST_ENTRY head = RemoveHeadList(&globals->m_ItemsHead);
		if (head != &globals->m_ItemsHead) {//ensure the list is not empty
			RemoteThreadItem* item = CONTAINING_RECORD(head, RemoteThreadItem, Entry);
			ExFreePool(item);
			globals->m_count--;
		}
	}
	PLIST_ENTRY current = globals->m_ItemsHead.Flink;
	while (current != &globals->m_ItemsHead) {
		if (current == entry) {// to prevent double insertion
			ExReleaseFastMutex(&globals->m_Lock);
			return FALSE;
		}
		current = current->Flink;
	}
	InsertHeadList(&globals->m_ItemsHead, entry);// the only thing that changes in respect of Globals_AddItem
	globals->m_count++;
	ExReleaseFastMutex(&globals->m_Lock);
	return TRUE;
}

LIST_ENTRY* Globals_RemoveItem(PGlobals globals) {
	ExAcquireFastMutex(&globals->m_Lock);

	PLIST_ENTRY entry = RemoveHeadList(&globals->m_ItemsHead);
	if (entry == &globals->m_ItemsHead) {//list is empty
		ExReleaseFastMutex(&globals->m_Lock);
		return NULL;
	}
	globals->m_count--;

	ExReleaseFastMutex(&globals->m_Lock);
	return entry; 
}

ULONG Globals_AddNewProcess(PGlobals globals, ULONG pid) {
	ExAcquireFastMutex(&globals->m_Lock);
	globals->NewProcesses[globals->m_count] = pid; 
	globals->m_count = (globals->m_count ++) % globals->m_MaxCount; // in pracice I hold the last process created in m_count - 1 
	ExReleaseFastMutex(&globals->m_Lock);
	return 0; 
}

ULONG checkPresenceInNewProcessList(PGlobals globals,ULONG pid) {
	ULONG index = 0; // 0 means is not found , 1-32 if found 
	ExAcquireFastMutex(&globals->m_Lock);
	for (ULONG i = 0; i < globals->m_MaxCount; i++) {
		if (globals->NewProcesses[i] == pid) {
			index = i+1; 
			break;
		}
	}
	ExReleaseFastMutex(&globals->m_Lock);
	return index; 
}

void RemoveProcessFromList(PGlobals globals, ULONG position) {
	ExAcquireFastMutex(&globals->m_Lock);
	globals->NewProcesses[position - 1] = 0; 
	globals->m_count--;
	ExReleaseFastMutex(&globals->m_Lock);
}