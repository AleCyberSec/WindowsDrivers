#include <ntifs.h> //must be put before include <ntddk.h>
#include <ntddk.h>
#include "BoosterCommon.h"


void BoosterUnload(PDRIVER_OBJECT DriverObject);

//this are dispatch routines 
NTSTATUS BoosterCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS BoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	DriverObject->DriverUnload = BoosterUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = BoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = BoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = BoosterDeviceControl;

	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Booster");
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed in IoCreateDevice (0x%X)\n", status));
		return status; 
	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Booster");
	status = IoCreateSymbolicLink(&symLink, &devName);

	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(DeviceObject); 
		KdPrint(("Failed in IoCreateSymbolicLink (0x%X)\n", status));
		return status; 
	}

	return STATUS_SUCCESS;
}

//do what has been done in DriverEntry, but in reverse
void BoosterUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Booster");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS BoosterCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0; 
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS; 
}

NTSTATUS BoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto dic = &stack->Parameters.DeviceIoControl;
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	switch (dic->IoControlCode) {
		
		case IOCTL_SET_PRIORITY:
			if (dic->InputBufferLength < sizeof(ThreadData)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			
			ThreadData* input = (ThreadData*)Irp->AssociatedIrp.SystemBuffer;
			if (input == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (input->priority < 1 || input->priority > 31) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			
			//the PsLookup function increments the reference count of the thread object, so it cannot die until further notice
			PETHREAD Thread; 
			status = PsLookupThreadByThreadId(UlongToHandle(input->ThreadId), &Thread);
			if (!NT_SUCCESS(status)) break;
			
			//KeSetPriorityThread returns the previous priority of the thread, but we do not care
			KeSetPriorityThread((PKTHREAD)Thread, input->priority);// is sufficient to do thid "(PKTHREAD)Thread" because the KTHREAD pointer is the first element of the ETHREAD structure
			ObfDereferenceObject(Thread); // decrements by one the reference count of the specified kernel object
			break;

	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;	
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;

}