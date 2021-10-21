/*++

Module Name:

    main.c

Abstract:

    This is the main module of the WdmCLient User-Mode console application.

Environment:

    User mode

--*/

#include "main.h"
#include "comm.h"


int
wmain(int argc, WCHAR *argv[])
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    __try
    {
        if (!InitComm(WDM_DEFAULT_THREAD_NO))
        {
            LOG_ERROR(0, L"InitComm failed!");
            __leave;
        }

        ProcessInput();

    }
    __finally
    {
        UninitComm();
    }

    return 0;
}

VOID
PrintHelp(
    VOID
)
{
    LOG_HELP(L"Commands:");
    LOG_HELP(L"%s        - show help", CMD_OPT_HELP);
    LOG_HELP(L"%s        - exit client", CMD_OPT_EXIT);

    return;
}

BOOLEAN
ParseCmd(
    _Out_ WCHAR Arguments[][MAX_PATH],
    _Out_ PDWORD ArgumentsNr
)
{
    DWORD   argsNr = 0;
    WCHAR   line[MAX_PATH];
    PWCHAR  cmd = NULL;
    PWCHAR  buffer = NULL;
    BOOLEAN bFailed = TRUE;

    __try
    {
        wprintf(L">");
        fgetws(line, MAX_PATH, stdin);

        cmd = wcstok_s(line, CMD_DELIMITER, &buffer);
        while (cmd)
        {
            if (argsNr >= CMD_MAX_ARGS)
            {
                LOG_WARN(L"Too many arguments. Max:[%d]", CMD_MAX_ARGS);
                __leave;
            }

            wcscpy_s(Arguments[argsNr++], MAX_PATH, cmd);
            cmd = wcstok_s(NULL, CMD_DELIMITER, &buffer);
        }

        bFailed = FALSE;
    }
    __finally
    {
        if (!bFailed)
        {
            *ArgumentsNr = argsNr;
        }
    }
     
    return !bFailed;
}


VOID
ProcessInput(
    VOID
)
{
    WCHAR       cmd[CMD_MAX_ARGS][MAX_PATH];
    BOOLEAN     bExit = FALSE;
    DWORD       cmdLen = 0;


    __try
    {
        do
        {
            ZeroMemory(cmd, sizeof(cmd));

            if (!ParseCmd(cmd, &cmdLen))
            {
                LOG_ERROR(0, L"parse_cmd failed");
                continue;
            }

            if (!wcscmp(cmd[0], CMD_OPT_EXIT))
            {
                bExit = TRUE;

                SendExitToDrv(gDevice);
            }
            else if (!wcscmp(cmd[0], CMD_OPT_HELP))
            {
                PrintHelp();
            }
            else if (!wcscmp(cmd[0], CMD_OPT_DUMP))
            {
                if (cmdLen != 2)
                {
                    LOG_WARN(L"expected 1 arg, found %d", cmdLen - 1);
                    continue;
                }
               
                if (!SendDumpToDrv(gDevice, cmd[1]))
                {
                    continue;
                }
            }
            else
            {
                LOG_WARN(L"Command [%s] not found", cmd[0]);
                PrintHelp();
            }

        } while (!bExit);

    }
    __finally
    {
    }

    return;
}

BOOLEAN
InitComm(
    _In_ DWORD NumberOfThreads
)
{
    DWORD   i   = 0;
    BOOLEAN bOk = FALSE;
    

    if (WDM_MAX_THREAD_NO < NumberOfThreads)
    {
        LOG_ERROR(0, L"NumberOfThreads: %d > max: %d", NumberOfThreads, WDM_MAX_THREAD_NO);
        return bOk;
    }

    __try
    {
        gDevice = CreateFile(
            L"\\\\.\\IOCTest",                      
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,     
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,                  
            NULL);
        if (gDevice == INVALID_HANDLE_VALUE)
        {
            LOG_ERROR(GetLastError(), L"CreateFile failed");
            __leave;
        }

        gTerminateThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!gTerminateThreadEvent)
        {
            LOG_ERROR(GetLastError(), L"CreateEvent failed");
            __leave;
        }

        for (i = 0; i < NumberOfThreads; ++i)
        {
            gThContext[i] = (PNOTIFICATION_CONTEXT)malloc(sizeof(NOTIFICATION_CONTEXT));
            if (!gThContext[i])
            {
                LOG_ERROR(GetLastError(), L"failed for PNOTIFICATION_CONTEXT");
                break;
            }
            ZeroMemory(gThContext[i], sizeof(gThContext[i]));

            gThContext[i]->Index = i;
            gThContext[i]->Ovlp.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            if (!gThContext[i]->Ovlp.hEvent)
            {
                LOG_ERROR(GetLastError(), L"CreateEvent failed");
                __leave;
            }

            gCommTh[i] = CreateThread(
                NULL, 
                0, 
                NotificationWatch, 
                (PVOID)gThContext[i],
                0, 
                NULL);
            if (!gCommTh[i])
            {
                LOG_ERROR(GetLastError(), L"CreateThread failed for NotificationWatch");
                __leave;
            }
        }

        bOk = TRUE;
    }
    __finally
    {
        if (bOk)
        {
            gThreadNo = i;
        }
    }

    return bOk;
}


VOID
UninitComm(
    VOID
)
{
    DWORD index;

    if (NULL != gTerminateThreadEvent)
    {
        SetEvent(gTerminateThreadEvent);
    }

    if (gThreadNo)
    {
        WaitForMultipleObjects(gThreadNo, gCommTh, TRUE, INFINITE);
    }
    for (index  = 0; index < gThreadNo; ++index)
    {
        CloseHandle(gCommTh[index]);
        gCommTh[index] = NULL;

        if (gThContext[index] != NULL)
        {
            CloseHandle(gThContext[index]->Ovlp.hEvent);
            gThContext[index]->Ovlp.hEvent = NULL;

            free(gThContext[index]);
            gThContext[index] = NULL;
        }
    }
    gThreadNo = 0;

    CloseHandle(gTerminateThreadEvent);
    gTerminateThreadEvent = NULL;

    CloseHandle(gDevice);

    return;
}


