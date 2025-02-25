#include <ntddk.h>

void ZeroDriverUnload(PDRIVER_OBJECT DriverObject);

NTSTATUS ZeroDriverCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS ZeroDriverRead(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS ZeroDriverWrite(PDEVICE_OBJECT, PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	
	DriverObject->DriverUnload = ZeroDriverUnload;
	
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ZeroDriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ZeroDriverCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = ZeroDriverRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroDriverWrite; 

	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Zero");
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed in IoCreateDevice (0x%X)\n", status));
		return status;
	}

	// Set DO_DIRECT_IO flag
	DeviceObject->Flags |= DO_DIRECT_IO;

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	status = IoCreateSymbolicLink(&symLink, &devName);

	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(DeviceObject);
		KdPrint(("Failed in IoCreateSymbolicLink (0x%X)\n", status));
		return status;
	}

	return STATUS_SUCCESS;
}


//do what has been done in DriverEntry, but in reverse
void ZeroDriverUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ZeroDriverCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0; 
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ZeroDriverRead(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG bytes = stack->Parameters.Read.Length;

	// Ensure we have a valid MDL (Direct I/O uses MDLs)
	PMDL mdl = Irp->MdlAddress;
	if (mdl == NULL) {
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

	ULONG mdlLength = MmGetMdlByteCount(mdl);
	if (bytes > mdlLength) {
		bytes = mdlLength;  // Prevent buffer overrun
	}

	// Get the system address for the locked pages
	PVOID buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
	if (buffer == NULL) {
		Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	// Zero the buffer
	RtlZeroMemory(buffer, bytes);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = bytes;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ZeroDriverWrite(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG bytes = stack->Parameters.Write.Length;
	
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = bytes;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

