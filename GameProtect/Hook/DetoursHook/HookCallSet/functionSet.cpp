#include "../../../dllmain.h"
#include "../../../../Common/Detours/Hook.h"
#include "functionSet.h"
#include "../../../Globals.h"


DWORD g_LastTickRPM;
DWORD g_LastTickWPM;

// Detour function (our hook)
BOOL WINAPI HookedWriteProcessMemory(
    HANDLE  hProcess,
    LPVOID  lpBaseAddress,
    LPCVOID lpBuffer,
    SIZE_T  nSize,
    SIZE_T* lpNumberOfBytesWritten
) {
    GAME_CLIENT game = GetSharedData();
    if (game.dwPid == GetProcessId(hProcess))
    {
        OutputDebugStringA("HookedWriteProcessMemory called");
        // MessageBox(NULL, L"You are attempting to hack the game using WriteProcessMemory.", L"HerculesAC", 0);
        char szBufKill[MAX_PATH] = { 0 };
        sprintf(szBufKill, "KILL GAME PID: %d", game.dwPid);
        OutputDebugStringA(szBufKill);
        if (!Common::TerminateWindowsProcess(Global::game.dwPid))
        {
            char szBufKillFailed[MAX_PATH] = { 0 };

            sprintf(szBufKillFailed, "KILL GAME PID: %d FAILED", game.dwPid);
            OutputDebugStringA(szBufKillFailed);
        }
    }

     return Sys_WriteProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten);
 
}

BOOL
WINAPI
NewReadProcessMemory(
    __in      HANDLE hProcess,
    __in      LPCVOID lpBaseAddress,
    __out_bcount_part(nSize, *lpNumberOfBytesRead) LPVOID lpBuffer,
    __in      SIZE_T nSize,
    __out_opt SIZE_T* lpNumberOfBytesRead)
{

    GAME_CLIENT game = GetSharedData();
    if (game.dwPid == GetProcessId(hProcess))
    {
        OutputDebugStringA("NewReadProcessMemory called");
        if (GetTickCount() - g_LastTickRPM < 60 * 1000)
        {
            ++Global::dwReadCount;
            char szBuf[MAX_PATH] = { 0 };
            sprintf(szBuf, "dwReadCount: %d", Global::dwReadCount);
            OutputDebugStringA(szBuf);
            if (Global::dwReadCount > 1000)
            {
                // MessageBox(NULL, L"You are attempting to hack the game using ReadProcessMemory.", L"HerculesAC", 0);
                char szBufKill[MAX_PATH] = { 0 };
                sprintf(szBufKill, "KILL GAME PID: %d", game.dwPid);
                OutputDebugStringA(szBufKill);
                if (!Common::TerminateWindowsProcess(Global::game.dwPid))
                {
                    char szBufKillFailed[MAX_PATH] = { 0 };

                    sprintf(szBufKillFailed, "KILL GAME PID: %d FAILED", game.dwPid);
                    OutputDebugStringA(szBufKillFailed);
               }
            }
        }
        else
        {
            g_LastTickRPM = GetTickCount();
        }
    }
    return Sys_ReadProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead);
}

void Hook_ReadProcessMemory()
{
    Sys_ReadProcessMemory = (PFN_READPROCESSMEMORY)ReadProcessMemory;
    assert(Sys_ReadProcessMemory);
    if (Sys_ReadProcessMemory)
    {
        HookOn((PVOID*)&Sys_ReadProcessMemory, NewReadProcessMemory, GetCurrentThread());
    }
}

void UnHook_ReadProcessMemory()
{
    if (Sys_ReadProcessMemory)
    {
        HookOff((PVOID*)&Sys_ReadProcessMemory, NewReadProcessMemory, GetCurrentThread());
    }
}


// ---------------------------------------

void Hook_WriteProcessMemory()
{
    Sys_WriteProcessMemory = (PFN_WRITEPROCESSMEMORY)WriteProcessMemory;
    assert(Sys_WriteProcessMemory);
    if (Sys_WriteProcessMemory)
    {
        HookOn((PVOID*)&Sys_WriteProcessMemory, HookedWriteProcessMemory, GetCurrentThread());
    }
}

void UnHook_WriteProcessMemory()
{
    if (Sys_WriteProcessMemory)
    {
        HookOff((PVOID*)&Sys_WriteProcessMemory, HookedWriteProcessMemory, GetCurrentThread());
    }
}