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

// The specified program was not found
void NoProgramFound(std::wstring sText)
{
	::MessageBox(NULL, sText.c_str(), _T("HAC Warning:"), MB_ICONWARNING | MB_SYSTEMMODAL);
	exit(0);
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

void LoadConfig()
{
	VMProtectionScope vmpScope;
	// Traverse the game client directory
	std::vector<std::wstring> GameDir = FileSystem::TraverseDirectory(Global::wsGamePath);

	std::wstring filename = FileSystem::GetModuleDirectory(NULL) + _T("hac.dat");
	logger.outDebug(_T("File path: %s"), filename.c_str());

	config[L"Client"] = FileSystem::ReadIniValue(filename, L"Game", L"Client");

	if (config[L"Client"].empty())
	{
		goto NoFound;
	}

	if (!FindProgram(GameDir, config[L"Client"]))
	{
		goto NoFound;
	}
	return;

NoFound:
	NoProgramFound(_T("The file is corrupted, please reinstall the game!"));
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