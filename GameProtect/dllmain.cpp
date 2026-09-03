#include "dllmain.h"
#include "Hook/MsgHook/MsgHook.h"
#include "Threads/ActiveThread.h"
#include "Hook/DetoursHook/HookCallSet/functionSet.h"
#include "Globals.h"
#include "../Common/Hash/MD5/MD5.h"

Logger logger;

std::map<std::wstring, std::wstring> fileMd5Value;


void ReportProcessInfo()
{
    std::wstring sInfo(L"[injection]");
    TCHAR szFileName[256];
    GetModuleFileName(NULL, szFileName, 256);
    sInfo += szFileName;
    OutputDebugString(sInfo.c_str());
}

GAME_CLIENT GetSharedData()
{
    GAME_CLIENT game = { 0 };
    try {
        SHARED_DATA data = g_sharedMemory->Read();
        g_sharedMemory->parseBuffer(data.ipc_msg.buffer, &game, sizeof(GAME_CLIENT));
        Global::game = game;
    }
    catch (const std::exception& e) {
        logger.Log(e.what());
    }
    return game;
}

extern "C" __declspec(dllexport) SharedMemory * GetSharedMemoryPtr()
{
    return g_sharedMemory;
}

BOOL InitIPC()
{
    try {
#ifdef _WIN64
        g_sharedMemory = InitializeSharedMemory(L"hacSharedMemory64");
#else
        g_sharedMemory = InitializeSharedMemory(L"hacSharedMemory");
#endif		
    }
    catch (const std::exception& e) {
        logger.Log(e.what());
        return FALSE;
    }
    return TRUE;
}

void CleanupSharedMemory()
{
    delete g_sharedMemory;
    g_sharedMemory = nullptr;
}

BOOL IsWhitelistCurrentProcess()
{
    TCHAR szFileName[256];
    GetModuleFileName(NULL, szFileName, 256);
    std::wstring fileMd5 = Common::stringToWideString(calculateMD5(Common::wideStringToString(szFileName)));
    if (std::any_of(fileMd5Value.begin(), fileMd5Value.end(), [fileMd5, szFileName](const auto& pair) {

        if (_wcsicmp(pair.second.c_str(), fileMd5.c_str()) == 0) 
        {
            logger.outDebug(_T("This CurrentProcess is Whitelist: %s; will skip hooking"), szFileName);
            return true;
        }
        else
        {
            return false;
        }
        }))
    {

        return TRUE;
    }
    return FALSE;
}

void SetupHook()
{
    Hook_ReadProcessMemory();
    Hook_WriteProcessMemory();
}

void UnHook()
{
    UnHook_ReadProcessMemory();
    UnHook_WriteProcessMemory();
}

void LoadConfig(HINSTANCE hinstDLL)
{
    std::wstring filename = FileSystem::GetModuleDirectory(hinstDLL) + _T("hac.dat");
    logger.outDebug(_T("File path: %s"), filename.c_str());
    int md5Count = Common::WStringToInt(FileSystem::ReadIniValue(filename, L"MD5", L"Count"));

    for (int i = 0; i < md5Count; i++)
    {
        fileMd5Value[L"MD5" + Common::IntToWString(i)] = FileSystem::ReadIniValue(filename, L"MD5", L"hash" + Common::IntToWString(i));
    }
}

void InitHook()
{
    if (!IsWhitelistCurrentProcess())
    {
         SetupHook();
    }
}

void Init(HINSTANCE hinstDLL)
{
    LoadConfig(hinstDLL);
    InitIPC();
    InitHook();
 }

void UnInit()
{
    UnHook();
    CleanupSharedMemory();
}


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
        //ReportProcessInfo();
        Init(hinstDLL);
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
        UnInit();
        break;
    }

    }
    return bRet;
}