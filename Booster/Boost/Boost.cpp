#include <stdio.h>
#include <Windows.h>
#include "..\Booster\BoosterCommon.h"

int main(int argc, char* argv[]) {
	
	if (argc < 3) {
		printf("Usage: Boost <tid> <priority>\n");
		return 0; 
	}

	int tid = atoi(argv[1]);
	int priority = atoi(argv[2]);

	HANDLE hDevice = CreateFileW(L"\\\\.\\Booster", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening device (%u)\n", GetLastError());
		return 1; 
	}

	ThreadData input; 
	input.ThreadId = tid; 
	input.priority = priority;
	DWORD bytesReturned;
	BOOL ok = DeviceIoControl(hDevice, IOCTL_SET_PRIORITY, &input, sizeof(input), nullptr, 0, &bytesReturned, nullptr);

	if (!ok) {
		printf("Error opening DeviceIoControl (%u)\n", GetLastError());
		return 1;
	}

	printf("Success!!");

	CloseHandle(hDevice);
}
