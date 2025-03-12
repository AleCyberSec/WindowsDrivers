#include <ntddk.h>
#include "..\ProcessHiding\Globals.h"
#include "..\ProcessHiding\Debug.h"
#include "..\ProcessHiding\ProcHide.h"

//global state
extern Globals g_state; 

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	
	UNREFERENCED_PARAMETER(RegistryPath);
	
	Globals_Init(&g_state);
	
	NTSTATUS status = STATUS_SUCCESS; 
	UNICODE_STRING  deviceName = RTL_CONSTANT_STRING(L"\\Device\\ProcHide");
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\ProcHide");
	PDEVICE_OBJECT DeviceObject = NULL; 

	do{
		status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device object (0x%08X)\n", status));
			break; 
		}
	
		status = IoCreateSymbolicLink(&symName, &deviceName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create symbolic link (0x%08X)\n", status));
			break;
		}

	} while (FALSE);

	if (!NT_SUCCESS(status)) {
		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}
		return status; 
	}

	DriverObject->DriverUnload = HideProcUnload; 
	DriverObject->MajorFunction[IRP_MJ_CREATE] = HideProcCreateClose; 
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = HideProcCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HideProcDeviceControl;
	return status; 
}

