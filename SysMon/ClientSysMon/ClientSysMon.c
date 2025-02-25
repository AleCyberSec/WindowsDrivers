#include <windows.h>
#include <stdio.h>
#include "..\SysMon\SysMonPublic.h"

#include <wchar.h>

#define MAX_DRIVE_NAME 3
#define MAX_TARGET_LENGTH 128



int Error(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

void DisplayInfo(BYTE* buffer, DWORD size);
void DisplayTime(const LARGE_INTEGER* time);
WCHAR* GetDosNameFromNTName(PCWSTR path);

int main() {
    HANDLE hFile = CreateFile(L"\\\\.\\SysMon", GENERIC_READ, 0,
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
        ClientItem* item = (ClientItem*)buffer;

        DisplayTime(&item->header.Time);

        /* 
        To timestamp in the correct format I need to see the header.size field that contains the size of the 
        EventData type (+ cmdLine in case of create)
        */

        switch (item->header.Type) {
        case ITEM_PROCESS_EXIT:
            printf("Process %u Exited (Code: %u)\n",
                item->data.processExitInfo.ProcessId,
                item->data.processExitInfo.ExitCode);
            break;

        case ITEM_PROCESS_CREATE: {
            printf("Process %u Created. Command line: %ws\n",
                item->data.processCreateInfo.ProcessId, item->data.processCreateInfo.CommandLine);
            break;
        }
        case ITEM_THREAD_CREATE: {
            printf("Thread %u of Process %u Creted\n",
                item->data.threadCreateInfo.ThreadId,
                item->data.threadCreateInfo.ProcessId);

            break; 
        }
        case ITEM_THREAD_EXIT: {
            printf("Thread %u of Process %u Exited (Code: %u)\n",
                item->data.threadExitInfo.ThreadId,
                item->data.threadExitInfo.ProcessId,
                item->data.threadExitInfo.ExitCode);
            break;
        }
        case ITEM_IMAGE_LOAD: {
            ImageLoadInfo* imageLoadInfo = &item->data.imageLoadInfo;
            printf("Image loaded into process %u at address 0x%llX (%ws)\n",
                imageLoadInfo->ProcessId, imageLoadInfo->LoadAddress,
                GetDosNameFromNTName(imageLoadInfo->ImageFileName));
            break;
        }

        default:
            fprintf(stderr, "Unknown item type: %d\n", item->header.Type);
            break;
        }

        // Move to the next event
        buffer = buffer + item->header.Size + sizeof(ItemHeader);
        size = size - item->header.Size - sizeof(ItemHeader);
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

// Translating from  
WCHAR* GetDosNameFromNTName(PCWSTR path) {
    static struct {
        WCHAR ntName[MAX_TARGET_LENGTH];
        WCHAR dosName[MAX_DRIVE_NAME];
    } driveMap[26];

    static int mapSize = 0;

    if (path[0] != L'\\') {
        return (WCHAR*)path;
    }

    if (mapSize == 0) {
        DWORD drives = GetLogicalDrives();
        WCHAR root[] = L"X:";
        WCHAR target[MAX_TARGET_LENGTH];

        //iterating through all the possible letters
        for (int c = 0; c < 26; c++) {
            if (drives & (1 << c)) {
                root[0] = L'A' + c;
                if (QueryDosDeviceW(root, target, _countof(target))) {
                    wcscpy_s(driveMap[mapSize].dosName, _countof(root), root);
                    wcscpy_s(driveMap[mapSize].ntName, _countof(target), target);
                    mapSize++;
                }
            }
        }
    }

    PCWSTR pos = wcschr(path + 1, L'\\');
    if (!pos) {
        return (WCHAR*)path;
    }

    pos = wcschr(pos + 1, L'\\');
    if (!pos) {
        return (WCHAR*)path;
    }

    WCHAR ntname[MAX_TARGET_LENGTH];
    wcsncpy_s(ntname, _countof(ntname), path, pos - path);

    for (int i = 0; i < mapSize; i++) {
        if (wcscmp(driveMap[i].ntName, ntname) == 0) {
            static WCHAR result[MAX_TARGET_LENGTH];
            wcscpy_s(result, _countof(result), driveMap[i].dosName);
            wcscat_s(result, _countof(result), pos);
            return result;
        }
    }

    return (WCHAR*)path;
}