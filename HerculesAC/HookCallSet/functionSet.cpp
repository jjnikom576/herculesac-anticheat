#include "../WinMain.h"
#include "../../Common/Detours/Hook.h"
#include "functionSet.h"
#include "../Globals.h"

typedef struct _TAG_THREAD_ENTRY
{
    LPTHREAD_START_ROUTINE lpStartAddress;
}THREAD_ENTRY, * PTHREAD_ENTRY;

// List of record thread entry
vectorExt<THREAD_ENTRY> ThreadEntryList;

__int64 __fastcall NewBaseThreadInitThunk(int a1, __int64(__fastcall* a2)(__int64), __int64 a3)
{
    if (!a1)
    {
        if ((a2 == (PVOID)LoadLibraryA) || (a2 == (PVOID)LoadLibraryW))
        {
            RtlExitUserThread(0);
            __debugbreak();  // If the thread fails to exit, an exception is triggered
        }

        BOOL boFound = FALSE;

        ThreadEntryList.Lock();
        for (const auto& threadAddr : ThreadEntryList)
        {
            if (threadAddr.lpStartAddress == (LPTHREAD_START_ROUTINE)a2)
            {
                boFound = TRUE;
                break;
            }
        }
        ThreadEntryList.UnLock();

        if (!boFound)
        {
            MessageBox(NULL, L"Suspected Thread Start", L"HerculesAC", MB_OK | MB_ICONINFORMATION);
            RtlExitUserThread(0);
            __debugbreak();  // If the thread fails to exit, an exception is triggered
        }
    }

    return BaseThreadInitThunk(a1, a2, a3);
}

void Hook_BaseThreadInitThunk()
{    
     assert(BaseThreadInitThunk);
    if (BaseThreadInitThunk)
    {
        HookOn((PVOID*)&BaseThreadInitThunk, NewBaseThreadInitThunk, GetCurrentThread());
    }
}

void UnHook_BaseThreadInitThunk()
{
    if (BaseThreadInitThunk)
    {
        HookOff((PVOID*)&BaseThreadInitThunk, NewBaseThreadInitThunk, GetCurrentThread());
    }
}


void InitFunctionPointer()
{
    RtlExitUserThread = (PFN_RTLEXITUSERTHREAD)GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "RtlExitUserThread");
    BaseThreadInitThunk = (PFN_BASETHREADINITTHUNK)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "BaseThreadInitThunk");
    Original_BaseThreadInitThunk = BaseThreadInitThunk;
}

HANDLE
WINAPI
NewCreateThread(
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
)
{
    THREAD_ENTRY threadAddr = { 0 };
    threadAddr.lpStartAddress = lpStartAddress;
    ThreadEntryList.Lock();
    ThreadEntryList.push_back(threadAddr);
    ThreadEntryList.UnLock();
    return Sys_CreateThread(lpThreadAttributes,
        dwStackSize,
        lpStartAddress,
        lpParameter,
        dwCreationFlags,
        lpThreadId);
}

void Hook_CreateThread()
{
    Sys_CreateThread = (PFN_CREATETHREAD)CreateThread;
    assert(Sys_CreateThread);
    if (Sys_CreateThread)
    {
        HookOn((PVOID*)&Sys_CreateThread, NewCreateThread, GetCurrentThread());
    }
}

void UnHook_CreateThread()
{
    if (Sys_CreateThread)
    {
        HookOff((PVOID*)&Sys_CreateThread, NewCreateThread, GetCurrentThread());
    }
}
 
void SetupHook()
{
     Hook_CreateThread();
     Hook_BaseThreadInitThunk();
}

void UnHook()
{
    UnHook_BaseThreadInitThunk();
    UnHook_CreateThread();
 }