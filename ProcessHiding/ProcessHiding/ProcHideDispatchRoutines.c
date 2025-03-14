#include <ntddk.h>
#include "..\ProcessHiding\ProcHide.h"
#include "..\ProcessHiding\Globals.h"

Globals g_state; 

//implementations of dispatch routines 
NTSTATUS HideProcCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
}


NTSTATUS HideProcDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG info = 0; 
	ULONG inputLen = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
	ULONG code = IrpSp->Parameters.DeviceIoControl.IoControlCode;

	ULONG outputLen = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;


	switch (code) {
		case IOCTL_UNHIDE_PROCESSES:
		case IOCTL_HIDE_PROCESSES :
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
			ULONG added; 
			if (code == IOCTL_HIDE_PROCESSES) {
				added = Globals_AddProcesses(&g_state, pids, count);
			} else {
				added = Globals_RemoveProcesses(&g_state, pids, count);
			}
			status = added == count ? STATUS_SUCCESS : STATUS_NOT_ALL_ASSIGNED; 
			info = added * sizeof(ULONG);
			break; 
		}
		case IOCTL_SHOW_HIDDEN_PROCESSES:
		{
			if (outputLen == 0 || Irp->MdlAddress == NULL) {
				status = STATUS_INVALID_PARAMETER;
				info = 0; 
				break; 
			}

			PVOID outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			
			if (!outputBuffer) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				info = 0; 
				break; 
			}

			//check on the len of the output buffer
			if (outputLen > (g_state.PidsCount * sizeof(ULONG))) {
				WriteHiddenProcessesInOutputBuffer((ULONG*)outputBuffer, &g_state);
				status = STATUS_SUCCESS;
				info = g_state.PidsCount * sizeof(ULONG);
				break;
			}
			else {
				status = STATUS_INSUFFICIENT_RESOURCES;
				info = 0;
				break; 
			}

		}

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;

	} 
	return CompleteRequest(Irp, status, info); 
}

void HideProcUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\ProcHide");
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);
}