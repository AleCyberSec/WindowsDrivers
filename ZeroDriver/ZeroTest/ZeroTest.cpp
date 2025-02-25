#include <stdio.h>
#include <Windows.h>

int main(int argc, char* argv[]) {

	HANDLE hDevice = CreateFileW(L"\\\\.\\Zero", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("Error opening device (%u)\n", GetLastError());
		return 1;
	}

	char test[13] = "test string\0";
    DWORD bytes_ret;
    // Write to the driver
    if (!WriteFile(hDevice, test, sizeof(test), &bytes_ret, NULL)) {
        printf("WriteFile failed: %d\n", GetLastError());
    }
    else {
        printf("WriteFile succeeded, bytes written: %d\n", bytes_ret);
    }

    // Read from the driver
    if (!ReadFile(hDevice, test, sizeof(test), &bytes_ret, NULL)) {
        printf("ReadFile failed: %d\n", GetLastError());
    }
    else {
        printf("ReadFile succeeded, bytes read: %d\n", bytes_ret);
       // printf("Data set to zeroes: %.*s\n", bytes_ret, test);  // Display the data read
        printf("test string: %s", test);
    }

	printf("Success!!");

	CloseHandle(hDevice);
}
