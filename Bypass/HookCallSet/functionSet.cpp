#include "../dllmain.h"
#include "../../Common/Detours/Hook.h"
#include "functionSet.h"
#include "../Globals.h"
#include "../Asm/AsmCallset.h"
HWND
WINAPI
NewFindWindowW(
    _In_opt_ LPCWSTR lpClassName,
    _In_opt_ LPCWSTR lpWindowName)
{
    if (lpClassName != NULL)
    {
        std::wstring titleName(lpClassName);
        if (_wcsicmp(titleName.c_str(), L"OLLYDBG") == 0) // Case-insensitive comparison
        {
            return NULL;
        }
    }
    return Sys_FindWindowW(lpClassName, lpWindowName);
}

HANDLE
WINAPI
NewCreateToolhelp32Snapshot(
    DWORD dwFlags,
    DWORD th32ProcessID
)
{
    return NULL;
}



void Hook_FindWindowW()
{
    Sys_FindWindowW = (PFN_FINDWINDOWW)FindWindowW;
    assert(Sys_FindWindowW);
    if (Sys_FindWindowW)
    {
        HookOn((PVOID*)&Sys_FindWindowW, NewFindWindowW, GetCurrentThread());
    }
}

void UnHook_FindWindowW()
{
    if (Sys_FindWindowW)
    {
        HookOff((PVOID*)&Sys_FindWindowW, NewFindWindowW, GetCurrentThread());
    }
}

void New_01_Crc32()
{
    Sleep(-1);
    /*
     ptr_FindWindowA = (ULONG64)FindWindowA;
    _New_01_Crc32();    
    pfn_01_Crc32();
    */
}

void Hook_01_Crc32()
{
    HMODULE hBac = GetModuleHandle(NULL);
    pfn_01_Crc32 = (PFN_ADDRESS)((BYTE*)hBac + 0x37645);
    assert(pfn_01_Crc32);
    if (pfn_01_Crc32)
    {
        HookOn((PVOID*)&pfn_01_Crc32, New_01_Crc32, GetCurrentThread());
    }
}

void UnHook_01_Crc32()
{
    if (pfn_01_Crc32)
    {
        HookOff((PVOID*)&pfn_01_Crc32, New_01_Crc32, GetCurrentThread());
    }
}

// Hooked function
DWORD WINAPI HookedCRC32(PVOID first_ptr, DWORD Size) {

    ptr_FindWindowA = (ULONG64)FindWindowA;

    // Call the original function
    return oCRC32((PVOID)ptr_FindWindowA, Size);

}



void Hook_02_Crc32()
{
    HMODULE hBac = GetModuleHandle(NULL);
    oCRC32 = (CRC32_t)((BYTE*)hBac + 0x37510);
    assert(oCRC32);
    if (oCRC32)
    {
        HookOn((PVOID*)&oCRC32, HookedCRC32, GetCurrentThread());
    }
}

void UnHook_02_Crc32()
{
    if (oCRC32)
    {
        HookOff((PVOID*)&oCRC32, HookedCRC32, GetCurrentThread());
    }
}


void Hook_CreateToolhelp32Snapshot()
{
    Sys_CreateToolhelp32Snapshot = (PFN_CREATETOOLHELP32SNAPSHOT)CreateToolhelp32Snapshot;
    assert(Sys_CreateToolhelp32Snapshot);
    if (Sys_CreateToolhelp32Snapshot)
    {
        HookOn((PVOID*)&Sys_CreateToolhelp32Snapshot, NewCreateToolhelp32Snapshot, GetCurrentThread());
    }
}

void UnHook_CreateToolhelp32Snapshot()
{
    if (Sys_CreateToolhelp32Snapshot)
    {
        HookOff((PVOID*)&Sys_CreateToolhelp32Snapshot, NewCreateToolhelp32Snapshot, GetCurrentThread());
    }
}