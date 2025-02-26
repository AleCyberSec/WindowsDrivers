#pragma once

//we pass this structure to the client 
typedef struct {
	LARGE_INTEGER Time;
	ULONG CreatorThreadId;
	ULONG CreatorProcessId; 
	ULONG CreatedProcessId; 
	ULONG CreatedThreadId; 
}RemoteThread;

typedef struct {
	LIST_ENTRY Entry;
	RemoteThread Remote;
}RemoteThreadItem;

#define DRIVER_PREFIX "RemoteThreadDetector driver: "
#define DRIVER_TAG 'TmeR'