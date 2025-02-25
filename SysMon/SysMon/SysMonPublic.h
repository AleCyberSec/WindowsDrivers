#pragma once

#define MAX_IMAGE_FILE_SIZE 300

typedef enum {
    ITEM_NONE,
    ITEM_PROCESS_CREATE,
    ITEM_PROCESS_EXIT, 
    ITEM_THREAD_CREATE, 
    ITEM_THREAD_EXIT, 
    ITEM_IMAGE_LOAD
} ItemType;

/*
The ItemHeader structure holds information common to all event types: the type of the event, the
time of the event (expressed as a 64-bit integer), and the size of the payload. The size is important, as
each event has its own information.
*/

typedef struct {
    ItemType Type;
    USHORT Size;
    LARGE_INTEGER Time;
} ItemHeader, * PItemHeader;

typedef struct {
    unsigned long ProcessId;
    unsigned long ExitCode;
} ProcessExitInfo, * PProcessExitInfo;

typedef struct {
    ULONG ProcessId;
    ULONG ParentProcessId;
    ULONG CreatingThreadId;
    ULONG CreatingProcessId;
    USHORT CommandLineLength;
    WCHAR CommandLine[1];
} ProcessCreateInfo, * PProcessCreateInfo;

typedef struct {
    ULONG ThreadId; 
    ULONG ProcessId; 
}ThreadCreateInfo, *PThreadCreateInfo;

typedef struct {
    ULONG ThreadId;
    ULONG ProcessId;
    ULONG ExitCode; 
}ThreadExitInfo, *PThreadExitInfo;

typedef struct {
    ULONG ProcessId;
    ULONG ImageSize;
    ULONG64 LoadAddress;
    WCHAR ImageFileName[MAX_IMAGE_FILE_SIZE + 1];
}ImageLoadInfo, *PImageLoadInfo;


/*
Now, we will define a union to hold different types of event data.
We use this structure to handle the data generically.
*/

typedef union {
    ProcessExitInfo processExitInfo;
    ProcessCreateInfo processCreateInfo;
    ThreadCreateInfo threadCreateInfo;
    ThreadExitInfo threadExitInfo;
    ImageLoadInfo imageLoadInfo;
} EventData, * PEventData;

typedef struct {
    ItemHeader header;  // Embed ItemHeader as a member
    LIST_ENTRY Entry;   // LIST_ENTRY for managing in a linked list
    EventData data;     // Union of event data types
} FullItem, * PFullItem;

/*
Now you have a single `FullItem` structure that can handle any event type by using the `EventData` union.
*/

typedef struct {
    ItemHeader header;
    EventData data;
}ClientItem, * PClientItem;

#define DRIVER_PREFIX "SYsMon driver: "
#define DRIVER_TAG 'msyS'