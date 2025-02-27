#include <ntddk.h>
#include "..\ProcessProtectorDriver\Globals.h"

void Globals_Init(PGlobals globals){
	ExInitializeResourceLite(&globals->Lock);
	globals->PidsCount = 0; 
	globals->RegHandle = NULL; 
	RtlZeroMemory(globals->Pids, sizeof(globals->Pids));
}

ULONG Globals_AddProcesses(PGlobals globals, const ULONG* pids, ULONG count) {
	ULONG added = 0;
	ULONG current = 0;
	BOOLEAN acquired = ExAcquireResourceExclusiveLite(&globals->Lock, TRUE);
	if (acquired) {
		for (ULONG i = 0; i < MAX_PIDS && added < count; i++) {
			if (globals->Pids[i] == 0) {
				globals->Pids[i] = pids[current++];
				added++;
			}
		}
		ExReleaseResourceLite(&globals->Lock);
	}
	globals->PidsCount += added;
	return added;
}

ULONG Globals_RemoveProcesses(PGlobals globals, const ULONG* pids, ULONG count) {
	ULONG removed = 0;
	BOOLEAN acquired = ExAcquireResourceExclusiveLite(&globals->Lock, TRUE);
	if (acquired) {
		for (int i = 0; i < MAX_PIDS && removed < count; i++) {
			ULONG pid = globals->Pids[i];
			if (pid) {
				for (ULONG c = 0; c < count; c++) {
					if (pid == pids[c]) {
						globals->Pids[i] = 0;
						removed++;
						break;
					}
				}
			}
		}
		ExReleaseResourceLite(&globals->Lock);
	}
	globals->PidsCount -= removed; 
	return removed; 
}

int Globals_FindProcess(PGlobals globals, ULONG pid) {
	int status = -1;
	BOOLEAN acquired = ExAcquireResourceSharedLite(&globals->Lock, TRUE);
	if (acquired) { 
		ULONG exist = 0;
		for (int i = 0; i < MAX_PIDS && exist < globals->PidsCount; i++) {
			if (globals->Pids[i] == pid) {
				status = i; 
				break; 
			}
			exist++;
		}
		ExReleaseResourceLite(&globals->Lock);
	}
	return status;
}

