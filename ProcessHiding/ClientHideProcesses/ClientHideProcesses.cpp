#include <Windows.h>               // For Windows API functions
#include <stdio.h>                 // For printf
#include <vector>                  // For std::vector
#include "..\ProcessHiding\Public.h" // For custom definitions (e.g., IOCTL codes)

// Function to print usage information (assuming PrintUsage is implemented elsewhere)
int PrintUsage() {
    printf("Usage: <command> <pids...>\n");
    printf("Commands:\n");
    printf("  add    - Add process IDs to protect\n");
    printf("  remove - Remove process IDs from protection\n");
    return -1;
}

// Function to print error messages (assuming Error is implemented elsewhere)
int Error(const char* message) {
    printf("%s\n", message);
    return -1;
}

// Function to parse command-line arguments into a vector of PIDs
std::vector<DWORD> ParsePids(const wchar_t* buffer[], int count) {
    std::vector<DWORD> pids;
    for (int i = 0; i < count; i++) {
        pids.push_back(_wtoi(buffer[i]));
    }
    return pids;
}

// Main function
int wmain(int argc, const wchar_t* argv[]) {
    if (argc < 2)
        return PrintUsage();

    // Enum for command options
    enum class Options {
        Unknown,
        Add, Remove
    };

    Options option = Options::Unknown;

    // Determine the action based on the command-line argument
    if (::_wcsicmp(argv[1], L"add") == 0)
        option = Options::Add;
    else if (::_wcsicmp(argv[1], L"remove") == 0)
        option = Options::Remove;
    else {
        printf("Unknown option.\n");
        return PrintUsage();
    }

    // Open the device
    HANDLE hFile = ::CreateFile(L"\\\\.\\ProcHide",
        GENERIC_WRITE | GENERIC_READ,
        0, nullptr,
        OPEN_EXISTING,
        0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return Error("Failed to open device");

    std::vector<DWORD> pids;
    BOOL success = FALSE;
    DWORD bytes;

    // Perform the requested action (Add, Remove, or Clear)
    switch (option) {
    case Options::Add:
        pids = ParsePids(argv + 2, argc - 2);
        success = ::DeviceIoControl(hFile, IOCTL_HIDE_PROCESSES,
            pids.data(), static_cast<DWORD>(pids.size()) * sizeof(DWORD),
            nullptr, 0, &bytes, nullptr);
        break;
    case Options::Remove:
        pids = ParsePids(argv + 2, argc - 2);
        success = ::DeviceIoControl(hFile, IOCTL_UNHIDE_PROCESSES,
            pids.data(), static_cast<DWORD>(pids.size()) * sizeof(DWORD),
            nullptr, 0, &bytes, nullptr);
        break;
    }

    // Check for success
    if (!success)
        return Error("Failed in DeviceIoControl");

    printf("Operation succeeded.\n");

    // Close the handle to the device
    ::CloseHandle(hFile);

    return 0;
}
