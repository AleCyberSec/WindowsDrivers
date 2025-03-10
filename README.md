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

## Notes for ProcessProtector driver
The callback defined and executed when a process is opened (removing the right of the handle to terminate the process) is not valid for processes that have user interface because by clicking the "X" they call the exit internally without opening any handles. 

## Notes for DelProtector driver
There are two ways to delete a file in Windows:
 - One way is to use IRP_MJ_SET_INFORMATION operation (can be done by the user mode APIs as SetFileInformationByHandle and kernel mode APIs such as NtSetInformationFile)
 - The other way is by opening a file with the FILE_DELETE_ON_CLOSE option flag. This flag can be set from user mode in CreateFile with FILE_FLAG_DELETE_ON_CLOSE. Higher level function DeleteFile uses the same flag behind the scenes. 