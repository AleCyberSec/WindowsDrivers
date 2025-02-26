#include <ntifs.h>
#include <ntddk.h>
#include "..\RemoteThreadDetector\Globals.h"
#include "..\RemoteThreadDetector\RThreadDetPublic.h"

Globals g_state; 

void RTDUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS RTDCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS RTDRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status = STATUS_SUCCESS; 

	PDEVICE_OBJECT DeviceObject = NULL; 
	UNICODE_STRING symlink = RTL_CONSTANT_STRING(L"\\??\\RTDdriver");
	BOOLEAN symlinkCreated = FALSE; 
	BOOLEAN ProcessCallbacks = FALSE; 

	do {
		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\RTDdriver");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to create device\n"));
			break; 
		}
		DeviceObject->Flags |= DO_DIRECT_IO;
		status = IoCreateSymbolicLink(&symlink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to create symlink\n"));
			break;
		}
		symlinkCreated = TRUE; 
		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to register process callback\n"));
			break;
		}
		ProcessCallbacks = TRUE; 
		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to register thread callback\n"));
			break;
		}
		
	} while (FALSE);
	
	if (!NT_SUCCESS(status)) {
		if (ProcessCallbacks) {
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		}
		if (symlinkCreated) {
			IoDeleteSymbolicLink(&symlink);
		}
		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}
	}

	Globals_Init(&g_state, 32);
	DriverObject->DriverUnload = RTDUnload; 
	DriverObject->MajorFunction[IRP_MJ_CREATE] = RTDCreateClose; 
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = RTDCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = RTDRead;
	return status; 
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status; 
	Irp->IoStatus.Information = info; 
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status; 
}

NTSTATUS RTDCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}

NTSTATUS RTDRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	ULONG length = irpSp->Parameters.Read.Length;
	NTSTATUS status = STATUS_SUCCESS; 
	ULONG bytes = 0; 

	NT_ASSERT(Irp->MdlAddress);
	PUCHAR buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		while (TRUE) {
			PLIST_ENTRY entry = Globals_RemoveItem(&g_state);
			if (entry == NULL)break;//No more Events 

			RemoteThreadItem* item = CONTAINING_RECORD(entry, RemoteThreadItem, Entry);
			if (length < sizeof(RemoteThread)) {
				Globals_AddHeadItem(&g_state, entry);
				break;
			}
			//copying he  data in the client buffer 
			memcpy((void*)buffer, &item->Remote, sizeof(RemoteThread));
			length -= sizeof(RemoteThread);
			buffer += sizeof(RemoteThread);
			bytes += sizeof(RemoteThread);

			ExFreePool(item);
		}
	}
	return CompleteRequest(Irp, status, bytes);
}

void RTDUnload(PDRIVER_OBJECT DriverObject) {
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	LIST_ENTRY* entry; 
	while ((entry = Globals_RemoveItem(&g_state)) != NULL) {
		ExFreePool(CONTAINING_RECORD(entry, RemoteThreadItem, Entry));
	}
	UNICODE_STRING symlink = RTL_CONSTANT_STRING(L"\\??\\RTDdriver");
	IoDeleteSymbolicLink(&symlink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	if (CreateInfo) {
		UNREFERENCED_PARAMETER(Process);
		Globals_AddNewProcess(&g_state, HandleToULong(ProcessId));
	}
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
	if (Create) {
		BOOLEAN remote = PsGetCurrentProcessId() != ProcessId && PsInitialSystemProcess != PsGetCurrentProcess()
			&& PsGetProcessId(PsInitialSystemProcess) != ProcessId;

		if (remote) {
			ULONG pos = checkPresenceInNewProcessList(&g_state, HandleToULong(ProcessId));
			if (pos) {
				RemoveProcessFromList(&g_state, pos);
			}
			else {

				RemoteThreadItem* item = (RemoteThreadItem*)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(RemoteThreadItem), DRIVER_TAG);
				if (item == NULL) {
					KdPrint((DRIVER_PREFIX "failed allocation"));
					return; 
				}

				//filling data
				item->Remote.CreatedProcessId = HandleToUlong(ProcessId);
				item->Remote.CreatedThreadId = HandleToUlong(ThreadId);
				item->Remote.CreatorProcessId = HandleToUlong(PsGetCurrentProcessId());
				item->Remote.CreatorThreadId = HandleToUlong(PsGetCurrentThreadId());
				KeQuerySystemTimePrecise(&item->Remote.Time);

				Globals_AddItem(&g_state, &item->Entry);
			}
		}
	}
}

// In OnProcessNotify I add the process in the process list and 
// if then I see a new thread of that process, I can delete that process from that list
// (because the first thread is created by the parent process). by removing from the list, I mean 
// In OnThreadnotify I add the element in the global structure if the process is not inside the process list.