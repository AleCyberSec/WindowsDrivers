#pragma once

typedef struct {
	LIST_ENTRY m_ItemsHead; 
	ULONG m_count; 
	ULONG m_MaxCount; 
	FAST_MUTEX m_Lock;
	ULONG NewProcesses[32];
}Globals, *PGlobals;

void Globals_Init(PGlobals globals, ULONG maxItems);
BOOLEAN Globals_AddItem(PGlobals globals, LIST_ENTRY* entry);
BOOLEAN Globals_AddHeadItem(PGlobals globals, LIST_ENTRY* entry);
LIST_ENTRY* Globals_RemoveItem(PGlobals globals);
ULONG Globals_AddNewProcess(PGlobals globals, ULONG pid);
ULONG checkPresenceInNewProcessList(PGlobals globals, ULONG pid); //return position+1 of the pid inside the array 
void RemoveProcessFromList(PGlobals globals, ULONG position);