#include <ntifs.h>
#include <ntddk.h>
#include <fltKernel.h>
#include "..\SysMon\SysMonPublic.h"
#include "..\SysMon\Globals.h"

Globals g_state;

void SysMonUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SysMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
void OnImageLoadNotify(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);
NTSTATUS OnRegistryNotify(PVOID context, PVOID arg1, PVOID arg2);


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {

	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status = STATUS_SUCCESS;

	PDEVICE_OBJECT DeviceObject = NULL;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	BOOLEAN symLinkCreated = FALSE;
	BOOLEAN ProcessCallbacks = FALSE; 
	BOOLEAN ThreadCallbacks = FALSE; 

	do {

		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\sysmon");
		status = IoCreateDevice(DriverObject, 0, &devName,
			FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);// Creating the device as EXCLUSIVE
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
			break;
		}
		DeviceObject->Flags |= DO_DIRECT_IO;
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create sym link (0x%08X)\n",
				status));
			break;
		}
		symLinkCreated = TRUE;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX
				"failed to register process callback (0x%08X)\n",
				status));
			break;
		}
		ProcessCallbacks = TRUE; 
		
		/*
		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to set thread callbacks (0x%08X)\n",
				status));
			break;
		}
		ThreadCallbacks = TRUE; 
		*/

		status = PsSetLoadImageNotifyRoutine(OnImageLoadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to set image callbacks (0x%08X)\n",
				status));
			break;
		}

		UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"7657.124");
		status = CmRegisterCallbackEx(OnRegistryNotify, &altitude, DriverObject,
			NULL, &g_state.m_RegCookie, NULL);
		
	} while (FALSE);

	if (!NT_SUCCESS(status)) {
		if (ThreadCallbacks) {
			PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
		}

		if (ProcessCallbacks) {
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		}
		
		if (symLinkCreated) {
			IoDeleteSymbolicLink(&symLink);
		}
		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}
	}
	Globals_Init(&g_state, 1000);

	DriverObject->DriverUnload = SysMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;
	return status;
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}
NTSTATUS SysMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
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

			if (entry == NULL) break; // No more events

			// Get event info
			PFullItem info = CONTAINING_RECORD(entry, FullItem, Entry);

			ULONG size = info->header.Size;  // Data size

			if (length < (size + sizeof(ItemHeader))) {
				// Not enough space in the buffer, put the item back in the list
				Globals_AddHeadItem(&g_state, entry);
				break;
			}

			// Copy ItemHeader and EventData
			memcpy((void*)buffer, &info->header, sizeof(ItemHeader));
			memcpy((void*)(buffer + sizeof(ItemHeader)), &info->data, size);
			
			/* For debugging
			PClientItem ex = (PClientItem) buffer; 
			
			if (ex == NULL) {
				// Assuming ex has some fields, you can reference them like:
				KdPrint(("Something"));
			}
			*/

			length -= sizeof(ItemHeader);
			length -= size;
			buffer = buffer + sizeof(ItemHeader) + size;
			bytes = bytes + sizeof(ItemHeader) + size;
			ExFreePool(info);  // Free allocated memory
		}
	}

	return CompleteRequest(Irp, status, bytes);
}

void SysMonUnload(PDRIVER_OBJECT DriverObject) {
	
	CmUnRegisterCallback(g_state.m_RegCookie);

	PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);

	/*PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);*/
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);

	// Wait for all callbacks to finish (to fix the error of unload the driver while there are callbacks running)
	while (InterlockedCompareExchange(&g_state.ActiveCallbacks, 0, 0) != 0) {
		LARGE_INTEGER delay;
		delay.QuadPart = -10 * 1000 * 100; // 100ms delay
		KeDelayExecutionThread(KernelMode, FALSE, &delay);
	}

	LIST_ENTRY* entry;
	while ((entry = Globals_RemoveItem(&g_state)) != NULL) {
		ExFreePool(CONTAINING_RECORD(entry, FullItem, Entry));
	}
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	InterlockedIncrement(&g_state.ActiveCallbacks);
	
	if (CreateInfo) {
		USHORT allocSize = sizeof(FullItem);
		USHORT commandLineSize = 0;

		if (CreateInfo->CommandLine) {
			commandLineSize = CreateInfo->CommandLine->Length; // in bytes
			allocSize += commandLineSize + sizeof(WCHAR); // Space for null terminator
		}

		// Allocate memory for FullItem
		PFullItem info = (PFullItem)ExAllocatePool2(POOL_FLAG_PAGED, allocSize, DRIVER_TAG);
		if (info == NULL) {
			KdPrint((DRIVER_PREFIX "failed allocation\n"));
			InterlockedDecrement(&g_state.ActiveCallbacks);
			return;
		}

		PItemHeader itemHeader = &info->header;
		PProcessCreateInfo itemInfo = &info->data.processCreateInfo;

		// Fill item header
		KeQuerySystemTimePrecise(&itemHeader->Time);
		itemHeader->Type = ITEM_PROCESS_CREATE;
		itemHeader->Size = sizeof(EventData) + commandLineSize;

		// Fill item data
		itemInfo->ProcessId = HandleToULong(ProcessId);
		itemInfo->ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		itemInfo->CreatingProcessId = HandleToULong(CreateInfo->CreatingThreadId.UniqueProcess);
		itemInfo->CreatingThreadId = HandleToULong(CreateInfo->CreatingThreadId.UniqueThread);

		// Copy command line if it exists
		if (commandLineSize > 0 && CreateInfo->CommandLine->Buffer) {
			itemInfo->CommandLineLength = commandLineSize / sizeof(WCHAR); // Number of characters
			memcpy(itemInfo->CommandLine, CreateInfo->CommandLine->Buffer, commandLineSize);
			itemInfo->CommandLine[itemInfo->CommandLineLength] = L'\0'; // Null-terminate
		}
		else {
			itemInfo->CommandLineLength = 0;
		}

		// Add to global list
		Globals_AddItem(&g_state, &info->Entry);
	}
	else {
		// Process exit
		PFullItem info = (PFullItem)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FullItem), DRIVER_TAG);
		if (info == NULL) {
			KdPrint((DRIVER_PREFIX "failed allocation\n"));
			return;
		}

		PItemHeader itemHeader = &info->header;
		PProcessExitInfo itemInfo = &info->data.processExitInfo;

		// Fill item header
		KeQuerySystemTimePrecise(&itemHeader->Time);
		itemHeader->Type = ITEM_PROCESS_EXIT;
		itemHeader->Size = sizeof(EventData);

		// Fill item data
		itemInfo->ProcessId = HandleToULong(ProcessId);
		itemInfo->ExitCode = PsGetProcessExitStatus(Process);

		// Add to global list
		Globals_AddItem(&g_state, &info->Entry);
	}
	InterlockedDecrement(&g_state.ActiveCallbacks);
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
	InterlockedIncrement(&g_state.ActiveCallbacks);

	PFullItem info = (PFullItem)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FullItem), DRIVER_TAG);
	if (info == NULL) {
		KdPrint((DRIVER_PREFIX "failed allocation\n"));
		InterlockedDecrement(&g_state.ActiveCallbacks);
		return;
	}

	ItemHeader* itemHeader = &info->header;
	EventData* itemInfo = &info->data;

	// Fill item header
	KeQuerySystemTimePrecise(&itemHeader->Time);
	itemHeader->Type = Create? ITEM_THREAD_CREATE : ITEM_THREAD_EXIT;
	itemHeader->Size = sizeof(EventData);
	
	// Fill item data 
	if (Create) {
		itemInfo->threadCreateInfo.ProcessId = HandleToULong(ProcessId);
		itemInfo->threadCreateInfo.ThreadId = HandleToULong(ThreadId);
	}
	else {
		itemInfo->threadExitInfo.ProcessId = HandleToULong(ProcessId);
		itemInfo->threadExitInfo.ThreadId = HandleToULong(ThreadId);
		
		PETHREAD thread;
		if (NT_SUCCESS(PsLookupThreadByThreadId(ThreadId, &thread))) {
			itemInfo->threadExitInfo.ExitCode = PsGetThreadExitStatus(thread);
			ObDereferenceObject(thread);
		}	 
	}

	Globals_AddItem(&g_state, &info->Entry);
	InterlockedDecrement(&g_state.ActiveCallbacks);
}

void OnImageLoadNotify(PUNICODE_STRING FullImageName,
	HANDLE ProcessId, PIMAGE_INFO ImageInfo) {
	if (ProcessId == NULL) {
		// system image, ignore
		return;
	}
	InterlockedIncrement(&g_state.ActiveCallbacks);

	PFullItem info = (PFullItem)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(FullItem), DRIVER_TAG);
	if (info == NULL) {
		KdPrint((DRIVER_PREFIX "failed allocation\n"));
		InterlockedDecrement(&g_state.ActiveCallbacks);
		return;
	}
	
	ItemHeader* itemHeader = &info->header;
	EventData* itemInfo = &info->data;

	// Fill item header
	KeQuerySystemTimePrecise(&itemHeader->Time);
	itemHeader->Type = ITEM_IMAGE_LOAD;
	itemHeader->Size = sizeof(EventData);

	itemInfo->imageLoadInfo.ProcessId = HandleToULong(ProcessId);
	itemInfo->imageLoadInfo.ImageSize = (ULONG)ImageInfo->ImageSize;
	itemInfo->imageLoadInfo.LoadAddress = (ULONG64)ImageInfo->ImageBase;

	if (ImageInfo->ExtendedInfoPresent) {
		// means that the structure I'm retrieving info
		// is a field of another structure with more info 
		PIMAGE_INFO_EX moreInfo = CONTAINING_RECORD(ImageInfo, IMAGE_INFO_EX, ImageInfo);
		PFLT_FILE_NAME_INFORMATION nameInfo; 
		if (NT_SUCCESS(FltGetFileNameInformationUnsafe(moreInfo->FileObject, NULL, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo))) {
			wcscpy_s(itemInfo->imageLoadInfo.ImageFileName, MAX_IMAGE_FILE_SIZE, nameInfo->Name.Buffer);
			FltReleaseFileNameInformation(nameInfo);
		}
	}
	if (itemInfo->imageLoadInfo.ImageFileName[0] == 0 && FullImageName) {
		wcscpy_s(itemInfo->imageLoadInfo.ImageFileName, MAX_IMAGE_FILE_SIZE, FullImageName->Buffer);
	}

	Globals_AddItem(&g_state, &info->Entry);
	InterlockedDecrement(&g_state.ActiveCallbacks);
}

//only care about writes done to HKEY_LOCAL_MACHINE
NTSTATUS OnRegistryNotify(PVOID context, PVOID arg1, PVOID arg2) {
	UNREFERENCED_PARAMETER(context);
	InterlockedIncrement(&g_state.ActiveCallbacks);

	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)arg1) {

		case RegNtPostSetValueKey: {
		REG_POST_OPERATION_INFORMATION* args = (REG_POST_OPERATION_INFORMATION*)arg2;
			if (!NT_SUCCESS(args->Status))
				break;
			static const WCHAR machine[] = L"\\REGISTRY\\MACHINE\\";
			PCUNICODE_STRING name;
			if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&g_state.m_RegCookie, args->Object,
				NULL, &name, 0))) {
				if (wcsncmp(name->Buffer, machine, ARRAYSIZE(machine) - 1) == 0) {
					REG_SET_VALUE_KEY_INFORMATION* preInfo = (REG_SET_VALUE_KEY_INFORMATION*)args->PreInformation;
					NT_ASSERT(preInfo);

					USHORT size = sizeof(RegistrySetValueInfo);
					USHORT keyNameLen = name->Length + sizeof(WCHAR);//for NULL termination 
					USHORT valueNameLen = preInfo->ValueName->Length + sizeof(WCHAR);
					USHORT valueSize = (USHORT)(preInfo->DataSize > 256 ? 256 : preInfo->DataSize);
					size += keyNameLen + valueNameLen + valueSize;

					PFullItem info = (PFullItem)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(ItemHeader) + sizeof(LIST_ENTRY) + size, DRIVER_TAG);
					if (info == NULL) {
						KdPrint((DRIVER_PREFIX "failed allocation\n"));
						InterlockedDecrement(&g_state.ActiveCallbacks);
						return STATUS_INSUFFICIENT_RESOURCES;
					}

					//filling header
					KeQuerySystemTimePrecise(&info->header.Time);
					info->header.Type = ITEM_REGISTRY_SET_VALUE;
					info->header.Size = size;

					//filling data
					info->data.registrySetValInfo.DataType = preInfo->Type;
					info->data.registrySetValInfo.DataSize = preInfo->DataSize;
					info->data.registrySetValInfo.ProcessId = HandleToULong(PsGetCurrentProcessId());
					info->data.registrySetValInfo.ThreadId = HandleToULong(PsGetCurrentThreadId());
					info->data.registrySetValInfo.ProvidedDataSize = valueSize;

					//filling the offsets
					USHORT offset = sizeof(RegistrySetValueInfo);
					info->data.registrySetValInfo.KeyNameOffset = offset; //The first offset starts at the end of the structure

					wcsncpy_s((PWSTR)((PUCHAR)&info->data.registrySetValInfo + offset),
						keyNameLen / sizeof(WCHAR), name->Buffer,
						name->Length / sizeof(WCHAR));

					offset += keyNameLen;
					info->data.registrySetValInfo.ValueNameOffset = offset;

					wcsncpy_s((PWSTR)((PUCHAR)&info->data.registrySetValInfo + offset),
						valueNameLen / sizeof(WCHAR), preInfo->ValueName->Buffer,
						preInfo->ValueName->Length / sizeof(WCHAR));

					offset += valueNameLen;
					info->data.registrySetValInfo.DataOffset = offset;
					memcpy((PUCHAR)&info->data.registrySetValInfo + offset, preInfo->Data, valueSize);

					// finally, add the item
					Globals_AddItem(&g_state, &info->Entry);
				}

				//if CmCallbackGetKeyObjectIDEx succeeds, the resulting key name must be explicitly freed
				CmCallbackReleaseKeyObjectIDEx(name);
			}
		}

	}
	InterlockedDecrement(&g_state.ActiveCallbacks);
	return STATUS_SUCCESS;
}