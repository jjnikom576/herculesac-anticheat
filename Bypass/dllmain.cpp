#include "dllmain.h"
#include "HookCallSet/functionSet.h"

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpReserved)   // reserved
{
    BOOL bRet = TRUE;


    // Perform actions based on the reason for calling.
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        OutputDebugString(L"DLL injection successful!");
        SetupHook();
        break;
    }

    case DLL_THREAD_ATTACH:
        // Do thread-specific initialization.
        break;

    case DLL_THREAD_DETACH:
    {
        // Do thread-specific cleanup.
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        // Perform any necessary cleanup.      
        UnHook();
        break;
    }

    }
    return bRet;
}


 void SetupHook()
{
    Hook_FindWindowW();
    Hook_01_Crc32();
    //Hook_02_Crc32();
    //Hook_CreateToolhelp32Snapshot();

}

 void UnHook()
{
    UnHook_FindWindowW();
    UnHook_01_Crc32();
    //UnHook_02_Crc32();
    //UnHook_CreateToolhelp32Snapshot();
}