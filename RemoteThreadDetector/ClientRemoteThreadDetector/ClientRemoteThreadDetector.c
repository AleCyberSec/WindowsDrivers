#include <windows.h>
#include <stdio.h>
#include "..\RemoteThreadDetector\RThreadDetPublic.h"

#include <wchar.h>

#define MAX_DRIVE_NAME 3
#define MAX_TARGET_LENGTH 128



int Error(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

void DisplayInfo(BYTE* buffer, DWORD size);
void DisplayTime(const LARGE_INTEGER* time);

int main() {
    HANDLE hFile = CreateFile(L"\\\\.\\RTDdriver", GENERIC_READ, 0,
        NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return Error("Failed to open file");

    const int size = 1 << 16; // 64 KB
    BYTE* buffer = (BYTE*)malloc(size);
    if (buffer == NULL) {
        CloseHandle(hFile);
        return Error("Failed to allocate memory");
    }

    while (1) {
        DWORD bytes = 0;
        if (ReadFile(hFile, buffer, size, &bytes, NULL)) {
            if (bytes > 0)
                DisplayInfo(buffer, bytes);
        }
        else {
            DWORD error = GetLastError();
            if (error == ERROR_MORE_DATA) {
                fprintf(stderr, "Buffer too small, consider increasing buffer size.\n");
                continue;
            }
            LPVOID msg;
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&msg, 0, NULL);
            fwprintf(stderr, L"ReadFile failed: %s\n", (LPWSTR)msg);
            LocalFree(msg);
            break;
        }

        Sleep(400); // wait a bit before polling again
    }

    free(buffer);
    CloseHandle(hFile);
    return 0;
}

void DisplayInfo(BYTE* buffer, DWORD size) {
    while (size > 0) {
        // I do the cast to ClientItem and not to FullItem
        // because I pass to the client app only the header and the data, and not the listEntry
        // so I defined the ClientItem which is a struct that holds this two components
        RemoteThread* item = (RemoteThread*)buffer;

        DisplayTime(&item->Time);

        /*
        To timestamp in the correct format I need to see the header.size field that contains the size of the
        EventData type (+ cmdLine in case of create)
        */

            printf("Thread %u of process %u creates a remote thread with id=%u in process %u\n",
                item->CreatorThreadId, item->CreatorProcessId, item->CreatedThreadId, item->CreatedProcessId);

        // Move to the next event
        buffer = buffer + sizeof(RemoteThread);
        size = size - sizeof(RemoteThread);
    }
}


void DisplayTime(const LARGE_INTEGER* time) {
    FILETIME local;
    FileTimeToLocalFileTime((FILETIME*)time, &local);
    SYSTEMTIME st;
    FileTimeToSystemTime(&local, &st);
    printf("%02d:%02d:%02d.%03d: ",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
