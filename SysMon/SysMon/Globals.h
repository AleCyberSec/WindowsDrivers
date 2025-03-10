#pragma once

typedef struct {
	LIST_ENTRY m_ItemsHead;
	ULONG m_Count;
	ULONG m_MaxCount;
	FAST_MUTEX m_Lock;
	LARGE_INTEGER m_RegCookie;
	LONG ActiveCallbacks;
}Globals, * PGlobals;

void Globals_Init(PGlobals globals, ULONG maxItems);
BOOLEAN Globals_AddItem(PGlobals globals, LIST_ENTRY* entry);
void Globals_AddHeadItem(PGlobals state, LIST_ENTRY* entry);
LIST_ENTRY* Globals_RemoveItem(PGlobals globals);