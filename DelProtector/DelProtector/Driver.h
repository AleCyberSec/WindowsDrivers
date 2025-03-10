#pragma once

typedef struct {
	PFLT_FILTER Filter;
	UNICODE_STRING Extensions; 
	ERESOURCE Lock; 
	PDRIVER_OBJECT DriverObject; 
} FilterState;

extern FilterState g_State; 

#define DRIVER_PREFIX "DelProtect driver: "
#define DRIVER_TAG 'torP'