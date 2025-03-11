#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

void DeleteUsingDeleteFile(const char* filename) {
    if (DeleteFileA(filename)) {
        printf("DeleteFile: Successfully deleted %s\n", filename);
    }
    else {
        printf("DeleteFile: Failed to delete %s (Error: %d)\n", filename, GetLastError());
    }
}

void DeleteUsingDeleteOnClose(const char* filename) {
    HANDLE hFile = CreateFileA(
        filename, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("DeleteOnClose: Failed to open %s (Error: %d)\n", filename, GetLastError());
        return;
    }

    printf("DeleteOnClose: File %s will be deleted when closed.\n", filename);
    CloseHandle(hFile); // File gets deleted when handle is closed
}

void DeleteUsingSetFileInformation(const char* filename) {
    HANDLE hFile = CreateFileA(
        filename, DELETE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("SetFileInformation: Failed to open %s (Error: %d)\n", filename, GetLastError());
        return;
    }

    FILE_DISPOSITION_INFO info;
    info.DeleteFile = TRUE;

    if (SetFileInformationByHandle(hFile, FileDispositionInfo, &info, sizeof(info))) {
        printf("SetFileInformation: Successfully marked %s for deletion.\n", filename);
    }
    else {
        printf("SetFileInformation: Failed to delete %s (Error: %d)\n", filename, GetLastError());
    }

    CloseHandle(hFile);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: deltest.exe <method> <filename>\n");
        printf("Method: 1 = DeleteFile, 2 = DeleteOnClose, 3 = SetFileInformation\n");
        return 1;
    }

    int method = atoi(argv[1]);
    const char* filename = argv[2];

    switch (method) {
    case 1:
        DeleteUsingDeleteFile(filename);
        break;
    case 2:
        DeleteUsingDeleteOnClose(filename);
        break;
    case 3:
        DeleteUsingSetFileInformation(filename);
        break;
    default:
        printf("Invalid method. Use 1, 2, or 3.\n");
        return 1;
    }

    return 0;
}
