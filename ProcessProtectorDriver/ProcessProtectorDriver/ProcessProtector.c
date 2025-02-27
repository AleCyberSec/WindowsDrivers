#include <ntddk.h>
#include "..\ProcessProtectorDriver\Globals.h"
#include "..\ProcessProtectorDriver\ProcessProtectorPublic.h"

Globals g_state;

void ProtectUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ProtectCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ProtectDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info);

OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID RegistrationContext,
	POB_PRE_OPERATION_INFORMATION Info);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {

	UNREFERENCED_PARAMETER(RegistryPath);
	
	Globals_Init(&g_state);
	
	OB_OPERATION_REGISTRATION operation = {
		PsProcessType,
		OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
		OnPreOpenProcess,
		NULL
	}; 

	OB_CALLBACK_REGISTRATION reg = {
		OB_FLT_REGISTRATION_VERSION,
		1,
		RTL_CONSTANT_STRING(L"12345.6171"),
		NULL,
		&operation
	};

	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\KProtect");
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\KProtect");
	PDEVICE_OBJECT DeviceObject = NULL;

	do {
		
		status = ObRegisterCallbacks(&reg, &g_state.RegHandle);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to register callbacks (0x%08X)\n",status));
			break;
		}
		status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN,
			0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device object (0x%08X)\n",
				status));
			break;
		}
		status = IoCreateSymbolicLink(&symName, &deviceName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create symbolic link (0x%08X)\n",
				status));
			break;
		}

	} while (FALSE);

	if(!NT_SUCCESS(status)) {
		if (g_state.RegHandle) {
			ObUnRegisterCallbacks(&g_state.RegHandle);
		}
		if (DeviceObject) {
			IoDeleteDevice(DeviceObject);
		}
		return status;
	}

	DriverObject->DriverUnload = ProtectUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ProtectCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ProtectCreateClose; 
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProtectDeviceControl;
	return status;
}

NTSTATUS ProtectDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG info = 0;
	ULONG inputLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG code = irpSp->Parameters.DeviceIoControl.IoControlCode;

	switch (code) {
	case IOCTL_PROTECT_ADD_PID:
	case IOCTL_PROTECT_REMOVE_PID:
	{
		if (inputLen == 0 || inputLen % sizeof(ULONG) != 0) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		ULONG* pids = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
		if (pids == NULL) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		ULONG count = inputLen / sizeof(ULONG);
		ULONG added = code == IOCTL_PROTECT_ADD_PID
			? Globals_AddProcesses(&g_state, pids, count) : Globals_RemoveProcesses(&g_state ,pids, count);
		status = added == count ? STATUS_SUCCESS : STATUS_NOT_ALL_ASSIGNED;
		info = added * sizeof(ULONG);
		break;
	}
	case IOCTL_PROTECT_REMOVE_ALL:
	{
		BOOLEAN acquired = ExAcquireResourceExclusiveLite(&g_state.Lock, TRUE);
		if (acquired) {
			RtlZeroMemory(g_state.Pids, sizeof(g_state.Pids));
			g_state.PidsCount = 0;
			ExReleaseResourceLite(&g_state.Lock);
		}
		status = STATUS_SUCCESS;
		break;
	}
	}

	return CompleteRequest(Irp, status, info);
}


OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION Info) {
	UNREFERENCED_PARAMETER(RegistrationContext);

	if (Info->KernelHandle)
		return OB_PREOP_SUCCESS;
	PEPROCESS process = (PEPROCESS)Info->Object;
	ULONG pid = HandleToULong(PsGetProcessId(process));
	if (Globals_FindProcess(&g_state, pid) != -1) {
		// found in list, remove terminate access
		Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
	}
	return OB_PREOP_SUCCESS;
}

NTSTATUS ProtectCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status; 
	Irp->IoStatus.Information = info; 
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status; 
}

void ProtectUnload(PDRIVER_OBJECT DriverObject) {
	if (g_state.RegHandle) {
		ObUnRegisterCallbacks(g_state.RegHandle);
	}
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\KProtect");
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);
}