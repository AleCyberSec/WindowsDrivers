#include <Windows.h>
#include <stdio.h>
#include "..\KMelody\MelodyPublic.h"
int main() {
	HANDLE hDevice = CreateFile(MELODY_SYMLINK, GENERIC_WRITE, 0,
		NULL, OPEN_EXISTING, 0, NULL);
	Note notes[10];
	for (int i = 0; i < _countof(notes); i++) {
		notes[i].Frequency = 400 + i * 30;
		notes[i].Duration = 500;
		notes[i].Repeat = 1;
		notes[i].Delay = 0;
	}
	DWORD bytes;
	DeviceIoControl(hDevice, IOCTL_MELODY_PLAY, notes, sizeof(notes),
		NULL, 0, &bytes, NULL);
	for (int i = 0; i < _countof(notes); i++) {
		notes[i].Frequency = 1200 - i * 100;
		notes[i].Duration = 300;
		notes[i].Repeat = 2;
		notes[i].Delay = 300;
	}
	DeviceIoControl(hDevice, IOCTL_MELODY_PLAY, notes, sizeof(notes),
		NULL, 0, &bytes, NULL);
	CloseHandle(hDevice);
	return 0;
}