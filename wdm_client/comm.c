#include "comm.h"

extern HANDLE gTerminateThreadEvent;

VOID
SendExitToDrv(
    HANDLE Device
)
{
    BOOL bSuccess = FALSE;

    bSuccess = DeviceIoControl(
        Device,                    // device to be queried
        (DWORD)IOCTL_EXIT,          // operation to perform
        NULL, 0,                    // no input
        NULL, 0,                    // no output 
        NULL,                       // # bytes returned
        NULL);                      // synchronous I/O          
    if (!bSuccess)
    {
        LOG_ERROR(GetLastError(), L"DeviceIoControl failed");
    }
}

BOOLEAN
SendDumpToDrv(
    HANDLE Device,
    PWCHAR Pid
)
{
    BOOL    bSuccess = FALSE;
    DWORD   noBytesReturned = 0;
    WCHAR   inBuf[IOC_BUFFER_MAX_SIZE] = L"";
    WCHAR   outBuf[IOC_BUFFER_MAX_SIZE] = L"";

    swprintf_s(inBuf, IOC_BUFFER_MAX_SIZE, L"%s", Pid);
    
    bSuccess = DeviceIoControl(
        Device,                                        // device to be queried
        (DWORD)IOCTL_DUMP_PROCESS,                     // operation to perform
        inBuf, IOC_BUFFER_MAX_SIZE * sizeof(WCHAR),    // input buffer
        outBuf, IOC_BUFFER_MAX_SIZE * sizeof(WCHAR),   // output buffer
        &noBytesReturned,                              // # bytes returned
        NULL);                                         // synchronous I/O        
    if (!bSuccess)
    {
        LOG_ERROR(GetLastError(), L"DeviceIoControl failed");
        return FALSE;
    }

    return TRUE;
}

DWORD WINAPI
NotificationWatch(
    LPVOID lpParam
)
{
    PPROC_INFO              outBuf = NULL;
    PNOTIFICATION_CONTEXT   context = NULL;
    BOOL                    bSuccess = FALSE;
    HANDLE                  events[MAXIMUM_WAIT_OBJECTS];
    DWORD                   waitRes = 0;
    DWORD                   lastErr = 0;

    assert(lpParam != NULL);

    context = (PNOTIFICATION_CONTEXT)lpParam;
    events[0] = gTerminateThreadEvent;
    events[1] = context->Ovlp.hEvent;

    __try
    {
        outBuf = (PPROC_INFO)malloc(sizeof(*outBuf));
        if (outBuf == NULL)
        {
            LOG_ERROR(GetLastError(), L"malloc failed");
            __leave;
        }

        for EVER
        {
            ZeroMemory(outBuf, sizeof(*outBuf));

            bSuccess = DeviceIoControl(
                gDevice,                        // device to be queried
                (DWORD)IOCTL_NOTIFY_CALLBACK,   // operation to perform
                NULL, 0,                        // no input buffer
                outBuf, sizeof(*outBuf),        // output buffer
                NULL,                           // # bytes returned
                &context->Ovlp);                // synchronous I/O        
            if (bSuccess)
            {
                LOG_ERROR(0, L"DeviceIoControl shoud have returned ERROR_IO_PENDING");
                __leave;

            }
            else
            {
                lastErr = GetLastError();
                if (lastErr != ERROR_IO_PENDING)
                {
                    // failed with something else
                    LOG_ERROR(lastErr, L"DeviceIoControl");
                    __leave;
                }
            }

            waitRes = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (waitRes == WAIT_OBJECT_0)
            {
                // Terminate Event was triggered
                if (!CancelIo(gDevice))
                {
                    LOG_ERROR(GetLastError(), L"CancelIo");
                }

                __leave;
            }
            else if (waitRes == WAIT_OBJECT_0 + 1)
            {
                // OVLP->hEvent: IO completed
            }
            else
            {
                LOG_ERROR(GetLastError(), L"WaitForMultipleObjects failed. waitRes:%d", waitRes);
                __leave;
            }

            LOG_INFO(L"%p %u", outBuf->ProcessId, outBuf->Create);

        } // <!> for EVER
    }
    __finally
    {
        if (outBuf != NULL)
        {
            free(outBuf);
            outBuf = NULL;
        }
    }

    return 0;
}