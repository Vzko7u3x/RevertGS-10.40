#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

LONG WINAPI DetourTransactionBegin(VOID);
LONG WINAPI DetourTransactionAbort(VOID);
LONG WINAPI DetourTransactionCommit(VOID);
LONG WINAPI DetourUpdateThread(HANDLE hThread);
LONG WINAPI DetourAttach(PVOID* ppPointer, PVOID pDetour);
LONG WINAPI DetourDetach(PVOID* ppPointer, PVOID pDetour);

#ifdef __cplusplus
}
#endif
