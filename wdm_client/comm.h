#pragma once
#include "main.h"

VOID
SendExitToDrv(
    HANDLE Device
);

BOOLEAN
SendDumpToDrv(
    HANDLE Device,
    PWCHAR Pid
);

DWORD WINAPI
NotificationWatch(
    LPVOID lpParam
);