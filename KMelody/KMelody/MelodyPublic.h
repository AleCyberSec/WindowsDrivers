#pragma once

#define MELODY_SYMLINK L"\\??\\KMelody"

typedef struct Note {
	ULONG Frequency;
	ULONG Duration;
	ULONG Delay;
	ULONG Repeat;
}Note, *PNote;

typedef struct _FullNote {
	Note base;            
	LIST_ENTRY Link;      
} FullNote, *PFullNote;

#define DRIVER_PREFIX "MelodyDriver: "
#define DRIVER_TAG 'gatM'  // 'Mtag' in ASCII (reversed due to little-endian storage)
#define MELODY_DEVICE 0x8003
#define IOCTL_MELODY_PLAY CTL_CODE(MELODY_DEVICE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)