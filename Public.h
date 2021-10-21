#pragma once


#define IOC_SYMBOLIC_LINK_NAME      L"\\Device\\IOC"
#define IOC_DEVICE_NAME             L"\\DosDevices\\IOCTest"

#define IOCTL_NOTIFY_CALLBACK       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DUMP_PROCESS          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER,  FILE_ANY_ACCESS)
#define IOCTL_EXIT                  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define IOC_BUFFER_MAX_SIZE         64



//
// Info provided by process notify
//
typedef struct _PROC_INFO
{
    HANDLE  ParentId;
    HANDLE  ProcessId;
    BOOLEAN Create;

}PROC_INFO, *PPROC_INFO;