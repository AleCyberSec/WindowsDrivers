#include <fltKernel.h>
#include <ntddk.h>

#include "..\DelProtector\Driver.h"
#include "..\DelProtector\DelProtectPublic.h"

FilterState g_State; 

NTSTATUS InitMiniFilter(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
NTSTATUS DelProtectInstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags,
	DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType);
NTSTATUS DelProtectUnload(FLT_FILTER_UNLOAD_FLAGS Flags);

NTSTATUS DelProtectInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);


void DelProtectInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason  // Use ULONG if FLT_INSTANCE_TEARDOWN_REASON is undefined
);

void DelProtectInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason
);

FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects, PVOID* pointer);
BOOLEAN IsDeleteAllowed(PUNICODE_STRING filename);

NTSTATUS OnCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS OnDeviceControl(PDEVICE_OBJECT devObj, PIRP Irp);
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	
	HANDLE hKey = NULL, hSubKey = NULL;
	NTSTATUS status;

	OBJECT_ATTRIBUTES keyAttr = RTL_CONSTANT_OBJECT_ATTRIBUTES(RegistryPath, OBJ_KERNEL_HANDLE);

	status = ZwOpenKey(&hKey, KEY_WRITE, &keyAttr);
	UNICODE_STRING subKey = RTL_CONSTANT_STRING(L"Instances");
	OBJECT_ATTRIBUTES subKeyAttr;
	InitializeObjectAttributes(&subKeyAttr, &subKey, OBJ_KERNEL_HANDLE, hKey,
		NULL);
	status = ZwCreateKey(&hSubKey, KEY_WRITE, &subKeyAttr, 0, NULL, 0, NULL);
	
	//
	// set "DefaultInstance" value. Any name is fine.
	//
	UNICODE_STRING valueName = RTL_CONSTANT_STRING(L"DefaultInstance");
	WCHAR name[] = L"DelProtectDefaultInstance";
	status = ZwSetValueKey(hSubKey, &valueName, 0, REG_SZ, name, sizeof(name));
	//
	// create "instance" key under "Instances"
	//
	UNICODE_STRING instKeyName;
	RtlInitUnicodeString(&instKeyName, name);
	HANDLE hInstKey;
	InitializeObjectAttributes(&subKeyAttr, &instKeyName, OBJ_KERNEL_HANDLE, hSubKey, NULL);
	status = ZwCreateKey(&hInstKey, KEY_WRITE, &subKeyAttr, 0, NULL, 0, NULL);
	
	//
	// write out altitude 
	//

	WCHAR altitude[] = L"425342";
	UNICODE_STRING altitudeName = RTL_CONSTANT_STRING(L"Altitude");
	status = ZwSetValueKey(hInstKey, &altitudeName, 0, REG_SZ, altitude, sizeof(altitude));
	
	//
	// write out flags
	//
	
	UNICODE_STRING flagsName = RTL_CONSTANT_STRING(L"Flags");
	ULONG flags = 0;
	status = ZwSetValueKey(hInstKey, &flagsName, 0, REG_DWORD, &flags, sizeof(flags));
	ZwClose(hInstKey);
	
	status = ExInitializeResourceLite(&g_State.Lock);

	if (!NT_SUCCESS(status)) {
		return status; 
	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\DelProtect");
	PDEVICE_OBJECT devObj = NULL;
	BOOLEAN symLinkCreated = FALSE; 
	
	do {
		
		status = InitMiniFilter(DriverObject, RegistryPath);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to Init mini-filter (0x%X)\n", status));
			break; 
		}

		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\DelProtect");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
		if (!NT_SUCCESS(status)) {
			break;
		}
		
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			break; 
		}
		symLinkCreated = TRUE;

		status = FltStartFiltering(g_State.Filter);
		if (!NT_SUCCESS(status)) {
			break; 
		}

	} while (FALSE);

	if (!NT_SUCCESS(status)) {
		ExDeleteResourceLite(&g_State.Lock);
		if (g_State.Filter) {
			FltUnregisterFilter(g_State.Filter);
		}
		if (symLinkCreated) {
			IoDeleteSymbolicLink(&symLink);
		}
		if (devObj) {
			IoDeleteDevice(devObj);
		}
		return status; 
	}

	g_State.DriverObject = DriverObject; 
	
	DriverObject->MajorFunction[IRP_MJ_CREATE] = OnCreateClose; 
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = OnCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnDeviceControl; 

	return status; 
}

NTSTATUS
DelProtectInstanceSetup(
	PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags,
	DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);
	return VolumeFilesystemType == FLT_FSTYPE_NTFS
		? STATUS_SUCCESS : STATUS_FLT_DO_NOT_ATTACH;
}

NTSTATUS DelProtectUnload(FLT_FILTER_UNLOAD_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	FltUnregisterFilter(g_State.Filter);
	ExDeleteResourceLite(&g_State.Lock);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\DelProtect");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(g_State.DriverObject->DeviceObject);
	return STATUS_SUCCESS;
}

NTSTATUS DelProtectInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
) {
	// Use these macros if you are not using these parameters yet
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	return STATUS_SUCCESS;
}

void DelProtectInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason  // Use ULONG if FLT_INSTANCE_TEARDOWN_REASON is undefined
) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Reason);

}

void DelProtectInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason
) {
	// Use these macros if you are not using these parameters yet
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Reason);

}

//returns FALSE if the extension is inside the g_State, otherwise TRUE
BOOLEAN IsDeleteAllowed(PUNICODE_STRING filename) {
	UNICODE_STRING ext; 
	if (NT_SUCCESS(FltParseFileName(filename, &ext, NULL, NULL))) {
		WCHAR uext[16] = { 0 };
		UNICODE_STRING suext; 
		suext.Buffer = uext; 
		//save space for null termination
		suext.MaximumLength = sizeof(uext) - 2 * sizeof(WCHAR);
		RtlUpcaseUnicodeString(&suext, &ext, FALSE);
		RtlAppendUnicodeToString(&suext, L";");

		//search for the prefix
		return wcsstr(g_State.Extensions.Buffer, uext) == NULL;
	}
	return TRUE; 

}

FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects, PVOID* pointer) {
	
	
	UNREFERENCED_PARAMETER(pointer);

	if (Data->RequestorMode == KernelMode) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	const FLT_PARAMETERS* params = &Data->Iopb->Parameters;
	NTSTATUS status = FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (params->Create.Options & FILE_DELETE_ON_CLOSE) {
		
		PUNICODE_STRING filename = &FltObjects->FileObject->FileName;
		KdPrint(("Delete On Close: %wZ\n", filename));

		if (!IsDeleteAllowed(filename)) {
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			status = FLT_PREOP_COMPLETE; 
			KdPrint(("(Pre Create) Prevent deletion of %wZ\n", filename));
		}

	}

	return status; 

}

FLT_PREOP_CALLBACK_STATUS DelProtectPreSetInformation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* pointer) {
	UNREFERENCED_PARAMETER(pointer);
	UNREFERENCED_PARAMETER(FltObjects);

	if (Data->RequestorMode == KernelMode) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	NTSTATUS status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	FLT_PARAMETERS* params = &Data->Iopb->Parameters;
	if (params->SetFileInformation.FileInformationClass == FileDispositionInformation ||
		params->SetFileInformation.FileInformationClass == FileDispositionInformationEx) {
		
		FILE_DISPOSITION_INFORMATION* info = (FILE_DISPOSITION_INFORMATION*)params->SetFileInformation.InfoBuffer;
		if (info->DeleteFile & 1) { // also covers FileDispositionInformationEx Flags
			PFLT_FILE_NAME_INFORMATION fi;

			//
			// using FLT_FILE_NAME_NORMALIZED is important here for parsing purposes
			//
			
			if (NT_SUCCESS(FltGetFileNameInformation(
				Data, FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED, &fi))) {
				if (!IsDeleteAllowed(&fi->Name)) {
					Data->IoStatus.Status = STATUS_ACCESS_DENIED;
					KdPrint(("(Pre Set Information) Prevent deletion of %wZ\n",
						&fi->Name));
					status = FLT_PREOP_COMPLETE;
				}
				FltReleaseFileNameInformation(fi);
			}

		}
	}
	return status; 

}

NTSTATUS InitMiniFilter(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {

	UNREFERENCED_PARAMETER(RegistryPath);

	WCHAR ext[] = L"PDF;";
	g_State.Extensions.Buffer = (PWSTR)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(ext), DRIVER_TAG);
	if (g_State.Extensions.Buffer == NULL) {
		return STATUS_NO_MEMORY; 
	}
	memcpy(g_State.Extensions.Buffer, ext, sizeof(ext));
	g_State.Extensions.Length = g_State.Extensions.MaximumLength = sizeof(ext);

	FLT_OPERATION_REGISTRATION const callbacks[] = {
		{IRP_MJ_CREATE, 0, DelProtectPreCreate, NULL},
		{IRP_MJ_SET_INFORMATION, 0, DelProtectPreSetInformation, NULL},
		{IRP_MJ_OPERATION_END}
	};

	FLT_REGISTRATION const reg = {
		sizeof(FLT_REGISTRATION),
		FLT_REGISTRATION_VERSION,
		0,
		NULL,
		callbacks,
		DelProtectUnload,
		DelProtectInstanceSetup,
		DelProtectInstanceQueryTeardown,
		DelProtectInstanceTeardownStart,
		DelProtectInstanceTeardownComplete,
	};

	NTSTATUS status = FltRegisterFilter(DriverObject, &reg, &g_State.Filter);

	return status; 
}

NTSTATUS OnDeviceControl(PDEVICE_OBJECT devObj, PIRP Irp) {
	UNREFERENCED_PARAMETER(devObj);

	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
	ULONG len = 0U; 
	WCHAR* ext = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
	ULONG inputLen = stack->Parameters.DeviceIoControl.InputBufferLength;

	switch (code) {
		
		case IOCTL_DELPROTECT_SET_EXTENSIONS:
			if (ext == NULL || inputLen < sizeof(WCHAR) * 2 || ext[inputLen / sizeof(WCHAR) - 1] != 0) {
				status = STATUS_INVALID_PARAMETER; 
				break; 
			}
			if (g_State.Extensions.MaximumLength < inputLen - sizeof(WCHAR)) {
				PVOID buffer = ExAllocatePool2(POOL_FLAG_PAGED, inputLen, DRIVER_TAG);
				if (buffer == NULL) {
					status = STATUS_INSUFFICIENT_RESOURCES;
					break; 
				}
				g_State.Extensions.MaximumLength = (USHORT)inputLen;

				ExFreePool(g_State.Extensions.Buffer);
				g_State.Extensions.Buffer = (PWSTR)buffer; 
			}
			UNICODE_STRING ustr; 
			RtlInitUnicodeString(&ustr, ext);
			RtlUpcaseUnicodeString(&ustr, &ustr, FALSE);
			memcpy(g_State.Extensions.Buffer, ext, inputLen);
			g_State.Extensions.Length = (USHORT)inputLen;
			len = inputLen; 
			status = STATUS_SUCCESS; 
			break; 

	}
	return CompleteRequest(Irp, status, len);
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS OnCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}