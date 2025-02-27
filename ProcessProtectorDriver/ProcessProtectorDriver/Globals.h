#pragma once

#define MAX_PIDS 256
#define PROCESS_TERMINATE 0x0001 

typedef struct {
	ULONG Pids[MAX_PIDS];
	ERESOURCE Lock;
	ULONG PidsCount;
	PVOID RegHandle;
}Globals, *PGlobals;

void Globals_Init(PGlobals globals);
ULONG Globals_AddProcesses(PGlobals globals, const ULONG* pids, ULONG count);
ULONG Globals_RemoveProcesses(PGlobals globals, const ULONG* pids, ULONG count);
int Globals_FindProcess(PGlobals globals, ULONG pid);
