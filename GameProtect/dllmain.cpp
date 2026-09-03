#include "dllmain.h"
#include "Threads/ActiveThread.h"
#include "Hook/DetoursHook/HookCallSet/functionSet.h"
#include "Globals.h"
#include <sodium.h>

Logger logger;

static hac::manifest::Manifest g_manifest;

static void WriteEventLog(const wchar_t* reason)
{
	HANDLE h = RegisterEventSourceW(NULL, L"HerculesAC");
	if (!h) return;
	LPCWSTR strings[1] = { reason };
	ReportEventW(h, EVENTLOG_ERROR_TYPE, 0 /*category*/, 1003 /*event id*/, NULL, 1, 0, strings, NULL);
	DeregisterEventSource(h);
}

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
	std::wstring root = FileSystem::GetModuleDirectory(hinstDLL);
	std::wstring manifest_path = root + L"hac.manifest";
	if (g_manifest.Load(manifest_path) != hac::manifest::ManifestError::Ok)
	{
		const char* reason = "GameProtect: manifest missing/malformed; refusing to hook";
		logger.Log(reason);
		WriteEventLog(Common::stringToWideString(reason).c_str());
		return;
	}
	uint8_t pk[32];
	std::memcpy(pk, hac::manifest::kEmbeddedPublicKey.data(), 32);
	if (g_manifest.VerifySignature(pk) != hac::manifest::ManifestError::Ok)
	{
		const char* reason = "GameProtect: manifest signature invalid; refusing to hook";
		logger.Log(reason);
		WriteEventLog(Common::stringToWideString(reason).c_str());
		return;
	}
}

void InitHook()
{
    // M3: targeted injection — GameProtect only loads in the game process.
    SetupHook();
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