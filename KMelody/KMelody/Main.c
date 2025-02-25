#include <ntddk.h>
#include "..\KMelody\MelodyPublic.h"
#include "..\KMelody\PlaybackState.h"

void MelodyUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS MelodyCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS MelodyDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);


PPlaybackState g_State;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	
	UNREFERENCED_PARAMETER(RegistryPath);

	g_State = ExAllocatePool2(POOL_FLAG_PAGED, sizeof(PlaybackState), DRIVER_TAG);

	if (g_State == NULL) return STATUS_INSUFFICIENT_RESOURCES;

	NTSTATUS status; 
	status = PlaybackState_Init(g_State);

	if (!NT_SUCCESS(status)) {
		return status; 
	}

	status = STATUS_SUCCESS;
	PDEVICE_OBJECT deviceObj = NULL;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\KMelody");
	do {
		UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\KMelody");
		status = IoCreateDevice(DriverObject, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObj);
		if (!NT_SUCCESS(status))break;
		status = IoCreateSymbolicLink(&symLink, &name);
		if (!NT_SUCCESS(status))break;

	} while (FALSE);

	if (!NT_SUCCESS(status)) {
		KdPrint((DRIVER_PREFIX "Error (0x%08X)\n", status));
		PlaybackState_Cleanup(g_State);
		if(deviceObj) IoDeleteDevice(deviceObj);
		return status;
	}

	DriverObject->DriverUnload = MelodyUnload; 
	DriverObject->MajorFunction[IRP_MJ_CREATE] = MelodyCreateClose; 
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = MelodyCreateClose; 
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MelodyDeviceControl;

	return STATUS_SUCCESS; 
}

void MelodyUnload(PDRIVER_OBJECT DriverObject) {
	PlaybackState_Cleanup(g_State);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\KMelody");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS MelodyDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG info = 0; 
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
	ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
	switch (code) {
	
		case IOCTL_MELODY_PLAY:
			if (inputLength == 0 || inputLength % sizeof(Note) != 0) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			PNote data = Irp->AssociatedIrp.SystemBuffer;
			if (data == NULL) {
				status = STATUS_INVALID_PARAMETER;
				break; 
			}
			status = PlaybackState_AddNotes(g_State, data, inputLength / sizeof(Note));
			if (!NT_SUCCESS(status))break; 
			info = inputLength; 
			break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS MelodyCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	NTSTATUS status = STATUS_SUCCESS;
	if (IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE) {
		status = PlaybackState_Start(g_State, DeviceObject);
	}
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}