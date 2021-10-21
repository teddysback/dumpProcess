#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <Windows.h>
#include <wchar.h>

#include "cmd_opts.h"
#include "..\Public.h"



#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__) // file name from where it was printed

#define LOG_HELP(M, ...)                wprintf_s(L"[HELP] " M L"\n", __VA_ARGS__)

#ifdef _DEBUG
    #define LOG_INFO(M, ...)            wprintf_s(L"[INFO] " M L"\n", __VA_ARGS__)
    #define LOG_WARN(M, ...)            wprintf_s(L"[WARN] (%S:%d:%S) " M L"\n" , __FILENAME__, __LINE__, __func__, __VA_ARGS__)
    #define LOG_ERROR(err, M, ...)      wprintf_s(L"[ERROR:0x%08x] (%S:%d:%S) " M L"\n", err, __FILENAME__, __LINE__, __func__, __VA_ARGS__)
#else
    #define LOG_INFO(M, ...)            wprintf_s(L"[INFO] " M L"\n", __VA_ARGS__)
    #define LOG_WARN(M, ...)            wprintf_s(L"[WARN] " M L"\n", __VA_ARGS__)
    #define LOG_ERROR(err, M, ...)      wprintf_s(L"[ERROR:0x%08x] " M L"\n", err, __VA_ARGS__)
#endif




BOOLEAN
InitComm(
    _In_ DWORD NumberOfThreads
);

VOID
UninitComm(
    VOID
);

VOID
PrintHelp(
    VOID
);

DWORD WINAPI
NotificationWatch(
    LPVOID lpParam
);

BOOLEAN
ParseCmd(
    _Out_ WCHAR Arguments[][MAX_PATH],
    _Out_ PDWORD ArgumentsNr
);

VOID
ProcessInput(
    VOID
);


#define WDM_MAX_THREAD_NO           MAXIMUM_WAIT_OBJECTS
#define WDM_DEFAULT_THREAD_NO       (1)

#define EVER                        (;;)
#define WHAT_THE_FUCK               while (TRUE)

//
//  The context information needed in the Notification thread
//
typedef struct _NOTIFICATION_CONTEXT
{
    DWORD           Index;
    OVERLAPPED      Ovlp;

}NOTIFICATION_CONTEXT, *PNOTIFICATION_CONTEXT;


HANDLE                  gTerminateThreadEvent;              // Signaled to terminate notification thread
HANDLE                  gCommTh[WDM_MAX_THREAD_NO];         // Notification thread handles
PNOTIFICATION_CONTEXT   gThContext[WDM_MAX_THREAD_NO];      // Notification thread contexts
DWORD                   gThreadNo;                          // Thread count
HANDLE                  gDevice;                            // Device Handle
