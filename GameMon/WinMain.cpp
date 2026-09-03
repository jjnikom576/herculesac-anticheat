#include "WinMain.h"
#include "Globals.h"

Logger logger;

PFN_SETUPMSGHOOK SetupMsgHook;
PFN_UNHOOKMSGHOOK UnHookMsgHook;
PFN_GETSHAREDMEMORYPTR GetSharedMemoryPtr;

namespace Global
{
	std::wstring wsGamePath;  // Game client directory
	DWORD dwPid;  // Game client pid
	HANDLE hProcess;  // Game client process handle
	HMODULE hGameProtect;
}

std::map<std::wstring, std::wstring> config;

// Shared game client information
void SharedGameClient()
{
	try {
		GAME_CLIENT game = { 0 };
		SHARED_DATA data = { 0 };
		data.ipc_msg.MsgId = IPC_GAME_CLIENT_ID;

		game.dwPid = Global::dwPid;
		wcscpy_s(game.szClient, config[L"Client"].c_str());
		wcscpy_s(game.szGamePath, Global::wsGamePath.c_str());
		g_sharedMemory->CopyToBuffer(game, data.ipc_msg.buffer, sizeof(data.ipc_msg.buffer));
		g_sharedMemory->Write(data);
	}
	catch (const std::exception& e) {
		logger.Log(e.what());
	}
}


void SetupHook()
{
	SetupMsgHook(Global::hGameProtect);
}

void UnHook()
{
	UnHookMsgHook();
}

void InitFunctionPtr()
{
#ifdef _WIN64
	Global::hGameProtect = LoadLibrary(L"./GameProtect64.dll");
#else
	Global::hGameProtect = LoadLibrary(L"./GameProtect.dll");
#endif
	SetupMsgHook = (PFN_SETUPMSGHOOK)GetProcAddress(Global::hGameProtect, "SetupMsgHook");
	UnHookMsgHook = (PFN_UNHOOKMSGHOOK)GetProcAddress(Global::hGameProtect, "UnHookMsgHook");
	GetSharedMemoryPtr = (PFN_GETSHAREDMEMORYPTR)GetProcAddress(Global::hGameProtect, "GetSharedMemoryPtr");
}

// Check if the game client is alive
BOOL CheckGameClientIsAlive()
{
	BOOL boRet = TRUE;
	if (!Common::IsProcessRunning(config[L"Client"]))
	{
		boRet = FALSE;
	}
	return boRet;
}

void StartServer()
{
	for (;;)
	{
		if (!CheckGameClientIsAlive())
		{
			break;
		}
		Sleep(5000);
	}
}

BOOL FindProgram(std::vector<std::wstring> files, std::wstring Program)
{
	for (const auto& file : files)
	{
		if (_wcsicmp(file.c_str(), Program.c_str()) == 0)  // Case-insensitive comparison
		{
			return TRUE;
		}
	}
	return FALSE;
}

static hac::manifest::Manifest g_manifest;

static void WriteEventLog(const wchar_t* reason)
{
	HANDLE h = RegisterEventSourceW(NULL, L"HerculesAC");
	if (!h) return;
	LPCWSTR strings[1] = { reason };
	ReportEventW(h, EVENTLOG_ERROR_TYPE, 0 /*category*/, 1002 /*event id*/, NULL, 1, 0, strings, NULL);
	DeregisterEventSource(h);
}

static void FailAndExit(const wchar_t* reason)
{
	WriteEventLog(reason);
	logger.Log((std::string("Manifest error: ") + Common::wideStringToString(reason)).c_str());
	::MessageBox(NULL, reason, _T("HAC Warning:"), MB_ICONWARNING | MB_SYSTEMMODAL);
	ExitProcess(1);
}

void LoadConfig()
{
	VMProtectionScope vmpScope;

	std::wstring root = FileSystem::GetModuleDirectory(NULL);
	std::wstring manifest_path = root + L"hac.manifest";

	if (g_manifest.Load(manifest_path) != hac::manifest::ManifestError::Ok)
	{
		FailAndExit(L"hac.manifest is missing or malformed.");
	}

	uint8_t pk[32];
	std::memcpy(pk, hac::manifest::kEmbeddedPublicKey.data(), 32);
	if (g_manifest.VerifySignature(pk) != hac::manifest::ManifestError::Ok)
	{
		FailAndExit(L"hac.manifest signature is invalid.");
	}

	config[L"Client"] = g_manifest.Game().client;
}

// Reclaim memory resources
void RecycResources()
{
}

void InitResources()
{
	g_sharedMemory = GetSharedMemoryPtr();
	SharedGameClient();
}

void Init(LPSTR lpCmdLine)
{
	std::string jsonString(lpCmdLine);

	// Parse the JSON string
	Json::CharReaderBuilder readerBuilder;
	Json::Value parsedRoot;
	std::string parseError;
	std::istringstream jsonStream(jsonString);
	bool parsingSuccessful = Json::parseFromStream(readerBuilder, jsonStream, &parsedRoot, &parseError);

	if (parsingSuccessful)
	{
		Global::wsGamePath = Common::stringToWideString(parsedRoot["GamePath"].asString());
		Global::dwPid = parsedRoot["pid"].asUInt();
		Global::hProcess = (HANDLE)parsedRoot["hProcess"].asUInt();

#ifdef _WIN64
		const wchar_t* mutexName = L"GameMonMutexName64";
#else
		const wchar_t* mutexName = L"GameMonMutexName";
#endif
		if (Common::SingletonPattern(mutexName))
		{
			LoadConfig();
			InitFunctionPtr();
			InitResources();
			SetupHook();
			StartServer();
			UnHook();
			Common::SingletonProgramEnd();
		}
	}
	else
	{

	}
}

int CALLBACK WinMain(
	_In_           HINSTANCE hInstance,
	_In_opt_       HINSTANCE hPrevInstance,
	_In_           LPSTR     lpCmdLine,
	_In_           int       nShowCmd
)
{
	VMProtectionScope vmpScope;

	if (lpCmdLine && lpCmdLine[0] != '\0')
	{
		logger.Log(lpCmdLine);
		Init(lpCmdLine);
	}
	else
	{
		logger.Log("The command line is empty.");
	}
	return 0;
}