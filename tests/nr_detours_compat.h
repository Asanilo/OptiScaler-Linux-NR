#pragma once
#include <windows.h>

#ifdef __MINGW32__
// Exact public declarations from the bundled Microsoft Detours 4.0.1 header.
// Its legacy _MSC_VER check redefines LONG_PTR on MinGW. Link the unmodified
// bundled library; these declarations only avoid that compiler-specific header.
extern "C"
{
    LONG WINAPI DetourTransactionBegin(VOID);
    LONG WINAPI DetourTransactionCommit(VOID);
    LONG WINAPI DetourUpdateThread(HANDLE hThread);
    LONG WINAPI DetourAttach(PVOID* ppPointer, PVOID pDetour);
}
#else
#include <detours/detours.h>
#endif
