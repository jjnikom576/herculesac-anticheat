#include "dllmain.h"
#include "Hook/MsgHook/MsgHook.h"
#include "Threads/ActiveThread.h"
#include "Hook/DetoursHook/HookCallSet/functionSet.h"
#include "Globals.h"
#include <sodium.h>
#include <fstream>

Logger logger;

static hac::manifest::Manifest g_manifest;


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
	if (g_manifest.WhitelistSha256().empty()) return FALSE;

	TCHAR szFileName[MAX_PATH] = {};
	GetModuleFileName(NULL, szFileName, MAX_PATH);

	// Hash the current process image with SHA-256 via libsodium.
	std::ifstream f(szFileName, std::ios::binary);
	if (!f) return FALSE;
	crypto_hash_sha256_state st;
	crypto_hash_sha256_init(&st);
	std::vector<uint8_t> buf(64 * 1024);
	while (f)
	{
		f.read(reinterpret_cast<char*>(buf.data()), buf.size());
		auto got = static_cast<size_t>(f.gcount());
		if (got == 0) break;
		crypto_hash_sha256_update(&st, buf.data(), got);
	}
	uint8_t h[crypto_hash_sha256_BYTES];
	crypto_hash_sha256_final(&st, h);

	static const char* d = "0123456789abcdef";
	std::string hex;
	hex.reserve(64);
	for (auto b : h)
	{
		hex.push_back(d[b >> 4]);
		hex.push_back(d[b & 0xF]);
	}

	for (const auto& allowed : g_manifest.WhitelistSha256())
	{
		if (allowed == hex)
		{
			logger.outDebug(_T("Whitelist match (%s); skipping hooks"), szFileName);
			return TRUE;
		}
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
	std::wstring root = FileSystem::GetModuleDirectory(hinstDLL);
	std::wstring manifest_path = root + L"hac.manifest";
	if (g_manifest.Load(manifest_path) != hac::manifest::ManifestError::Ok)
	{
		logger.Log("GameProtect: manifest missing/malformed; refusing to hook");
		return;
	}
	uint8_t pk[32];
	std::memcpy(pk, hac::manifest::kEmbeddedPublicKey.data(), 32);
	if (g_manifest.VerifySignature(pk) != hac::manifest::ManifestError::Ok)
	{
		logger.Log("GameProtect: manifest signature invalid; refusing to hook");
		return;
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