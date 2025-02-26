# Windows Drivers
For now I'm following the Second Edition of Windows Kernel Programming book.
I'm developing all the drivers in C differently from the book.

## Notes for RemoteThreadDetecor driver
In the solution I have 4 projects: 
1)Driver project (RemoteThreadDetector.sys)
2)Client that communicates with the Driver by opening the handle to the device defined in the DriverEntry function (ClientRemoteThreadDetector.c)
3)Executable that runs a CreateRemoteThread function (ExecutableTriggeringRemThread.c)
 The path of the dll must be changed properly inside this executable.
4)The dll to inject that shows the popup (dllMain.cpp)
 
