#include <Windows.h>
#include <stdio.h>
#include <winternl.h>
#include <ntddbeep.h>

#pragma comment(lib, "ntdll")

int main(int argc, const char* argv[]) {

	printf("beep [<frequency> <duration_in_msec>]\n");
	int freq = 800, duration = 1000;

	if (argc > 2) {
		freq = atoi(argv[1]);
		duration = atoi(argv[2]);
	}

	HANDLE hFile; 
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name; 
	RtlInitUnicodeString(&name, L"\\Device\\Beep");
	InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
	IO_STATUS_BLOCK ioStatus;
	NTSTATUS status = ::NtOpenFile(&hFile, GENERIC_WRITE, &attr, &ioStatus, 0, 0);

	if (NT_SUCCESS(status)) { 
		//the input buffer passed to DeviceIOControl is a structure called BEEP_SET_PARAMETERS that contains frequency and duration
		BEEP_SET_PARAMETERS params;
		params.Frequency = freq;
		params.Duration = duration;
		DWORD bytes;
		printf("Playing freq: %u, duration: %u\n", freq, duration);
		::DeviceIoControl(hFile, IOCTL_BEEP_SET, &params, sizeof(params), nullptr, 0, &bytes, nullptr);
		::Sleep(duration);
		::CloseHandle(hFile);
	}
}