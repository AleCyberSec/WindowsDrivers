#pragma once
#include "MelodyPublic.h"

typedef struct _PlaybackState {
	LIST_ENTRY m_head; // head of a linked list holding the notes to play, must be protected with a synchronization object 
	FAST_MUTEX m_lock;
	PAGED_LOOKASIDE_LIST m_lookaside; // the Note objects will be allocated from a lookaside list (each note has a fixed size) 
	KSEMAPHORE m_counter;
	KEVENT m_stopEvent;
	HANDLE m_hThread;
}PlaybackState, *PPlaybackState;

// Function prototypes
NTSTATUS PlaybackState_Init(PPlaybackState state);
void PlaybackState_Cleanup(PPlaybackState state);
NTSTATUS PlaybackState_AddNotes(PPlaybackState state, const Note* notes, ULONG count);
NTSTATUS PlaybackState_Start(PPlaybackState state, PVOID IoObject);
void PlaybackState_Stop(PPlaybackState state);
void PlaybackState_PlayMelody(PVOID context);
