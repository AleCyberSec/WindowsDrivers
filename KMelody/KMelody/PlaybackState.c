#include <ntddk.h>
#include "..\KMelody\PlaybackState.h"
#include "..\KMelody\MelodyPublic.h"
#include <ntddbeep.h>

// Initialize the PlaybackState structure, PlaybackState constructor
NTSTATUS PlaybackState_Init(PPlaybackState state) {
    if (!state) return STATUS_INVALID_PARAMETER;

    // Initialize the list, synchronization objects, and semaphore
    InitializeListHead(&state->m_head);
    KeInitializeSemaphore(&state->m_counter, 0, 1000); // maximum of 1000 Notes to be queued
    KeInitializeEvent(&state->m_stopEvent, SynchronizationEvent, FALSE);
    ExInitializeFastMutex(&state->m_lock);
    ExInitializePagedLookasideList(&state->m_lookaside, NULL, NULL, 0, sizeof(FullNote), DRIVER_TAG , 0);
    state->m_hThread = NULL;

    return STATUS_SUCCESS;
}

//PlaybackState destructor
void PlaybackState_Cleanup(PPlaybackState state) {
    
    PlaybackState_Stop(state); // deletes the thread created 
    ExDeletePagedLookasideList(&state->m_lookaside);

}

NTSTATUS PlaybackState_AddNotes(PPlaybackState state, const Note* notes, ULONG count) {
    KdPrint((DRIVER_PREFIX "State::AddNotes %u\n", count));
    for (ULONG i = 0; i < count; i++) {
        PFullNote fullNote = (PFullNote)ExAllocateFromPagedLookasideList(&state->m_lookaside);
        if (fullNote == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        memcpy(&fullNote->base, &notes[i], sizeof(Note));

        // Lock before modifying the shared list
        ExAcquireFastMutex(&state->m_lock);
        InsertTailList(&state->m_head, &fullNote->Link);//CRITICAL SECTION 
        ExReleaseFastMutex(&state->m_lock);
    }

    //
    // make the semaphore signaled (if it wasn't already) to
    // indicate there are new note(s) to play
    //
    KeReleaseSemaphore(&state->m_counter, 2, count, FALSE);

    KdPrint((DRIVER_PREFIX "Semaphore count: %u\n", KeReadStateSemaphore(&state->m_counter)));
    
    return STATUS_SUCCESS;
}

NTSTATUS PlaybackState_Start(PPlaybackState state, PVOID IoObject) {
        if (state == NULL || IoObject == NULL) {
            return STATUS_INVALID_PARAMETER;
        }

        // Lock to ensure only one thread is created
        ExAcquireFastMutex(&state->m_lock);

        // Check if the thread is already running
        if (state->m_hThread) {
            ExReleaseFastMutex(&state->m_lock);
            return STATUS_SUCCESS;
        }

        // Create a system thread
        NTSTATUS status = IoCreateSystemThread(
            IoObject,               // Driver or device object
            &state->m_hThread,      // Resulting handle
            THREAD_ALL_ACCESS,      // Access mask
            NULL,                   // No object attributes required
            NtCurrentProcess(),     // Create in the current process
            NULL,                   // Returned client ID
            PlaybackState_PlayMelody, // Thread function
            state                   // Passed to thread function
        );

        // Unlock after thread creation attempt
        ExReleaseFastMutex(&state->m_lock);

        return status;
}

void PlaybackState_PlayMelody(PVOID context) {
    PDEVICE_OBJECT beepDevice;
    UNICODE_STRING beepDeviceName = RTL_CONSTANT_STRING(DD_BEEP_DEVICE_NAME_U);
    PFILE_OBJECT beepFileObject;
    NTSTATUS status = IoGetDeviceObjectPointer(&beepDeviceName, GENERIC_WRITE, &beepFileObject, &beepDevice);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVER_PREFIX "Failed to locate beep device (0x%X)\n", status));
        return;
    }
    PPlaybackState ctx = (PPlaybackState)context;
    PVOID objects[] = { &ctx->m_counter, &ctx->m_stopEvent };
    IO_STATUS_BLOCK ioStatus;
    BEEP_SET_PARAMETERS params;

    for (;;) {
        status = KeWaitForMultipleObjects(2, objects, WaitAny, Executive, KernelMode, FALSE, NULL, NULL);
        if (status == STATUS_WAIT_1) {
            KdPrint((DRIVER_PREFIX "Stop event signaled. Exiting thread...\n"));
            break;
        }

        KdPrint((DRIVER_PREFIX "Semaphore count: %u\n", KeReadStateSemaphore(&ctx->m_counter)));

        PLIST_ENTRY link;

        // Acquire lock (equivalent to Locker locker(m_lock);)
        ExAcquireFastMutex(&ctx->m_lock);

        link = RemoveHeadList(&ctx->m_head);

        // Ensure we didn't remove the list head itself (list is not empty)
        NT_ASSERT(link != &ctx->m_head);

        // Release lock after modifying the list
        ExReleaseFastMutex(&ctx->m_lock);

        // Convert the list entry to a FullNote structure
        PFullNote note = CONTAINING_RECORD(link, FullNote, Link);

        if (note->base.Frequency == 0) {
            //
            // just do a delay
            //
            NT_ASSERT(note->base.Duration > 0);
            LARGE_INTEGER interval;
            interval.QuadPart = -10000LL * note->base.Duration;
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
        }
        else {
            //If the frequency in the note is not zero, then we need to call the Beep driver with proper IRP.
            params.Duration = note->base.Duration;
            params.Frequency = note->base.Frequency;
            int count = note->base.Repeat ? note->base.Repeat : 1;

            KEVENT doneEvent;
            KeInitializeEvent(&doneEvent, NotificationEvent, FALSE);
            for (int i = 0; i < count; i++) {

                PIRP irp = IoBuildDeviceIoControlRequest(IOCTL_BEEP_SET, beepDevice,
                    &params, sizeof(params), NULL, 0, FALSE, &doneEvent, &ioStatus);

                if (!irp) {
                    KdPrint((DRIVER_PREFIX "Failed to allocate IRP\n"));
                    break;
                }
                status = IoCallDriver(beepDevice, irp);
                if (!NT_SUCCESS(status)) {
                    KdPrint((DRIVER_PREFIX "Beep device playback error (0x%X)\n",
                        status));
                    break;
                }
                if (status == STATUS_PENDING) {
                    KeWaitForSingleObject(&doneEvent, Executive, KernelMode,
                        FALSE, NULL);
                }
                LARGE_INTEGER delay;
                delay.QuadPart = -10000LL * note->base.Duration;
                KeDelayExecutionThread(KernelMode, FALSE, &delay);

                if (i < count - 1 && note->base.Delay != 0) {
                    delay.QuadPart = -10000LL * note->base.Delay;
                    KeDelayExecutionThread(KernelMode, FALSE, &delay);
                }
            }
        }
        ExFreeToPagedLookasideList(&ctx->m_lookaside, note);
    }
    //When the stop event is signaled
    ObDereferenceObject(beepFileObject);
}
void PlaybackState_Stop(PPlaybackState state) {
    if (state->m_hThread) {
        //
        // Signal the thread to stop
        //
        KeSetEvent(&state->m_stopEvent, 2, FALSE);

        //
        // Wait for the thread to exit
        //
        PVOID thread;
        NTSTATUS status = ObReferenceObjectByHandle(
            state->m_hThread,  // Handle to the thread
            SYNCHRONIZE,       // Synchronization access
            *PsThreadType,     // Object type
            KernelMode,        // Running in kernel mode
            &thread,           // Receives thread pointer
            NULL               // Optional OBJECT_HANDLE_INFORMATION
        );

        if (!NT_SUCCESS(status)) {
            KdPrint((DRIVER_PREFIX "ObReferenceObjectByHandle error (0x%X)\n", status));
        }
        else {
            // Wait for the thread to terminate
            KeWaitForSingleObject(thread, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(thread);  // Dereference the thread object
        }

        // Close the thread handle
        ZwClose(state->m_hThread);
        state->m_hThread = NULL;
    }
}