#include "WinMain.h"
 
Logger logger("hercules_log.txt");


static const std::unordered_set<std::wstring> detectedProcessNames = {
	L"ollydbg",
	L"Dbgview",
	L"x32dbg",
	L"x64dbg",
	L"cheatengine",
	L"Picker",
	L"WPE.exe"
};

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


static const std::vector<std::pair<std::wstring, std::wstring>> detectedWindowInfo = {
	{ L"OLLYDBG", L"" },
	{ L"Dbgview", L"" },
	{ L"x32dbg", L"" },
	{ L"x64dbg", L"" },
	{ L"cheatengine", L"" },
	{ L"Picker", L"" },
	{ L"WPE.exe", L"" }
};


std::map<std::wstring, std::wstring> config;

void ReportIllegalInfo(std::string sInfo)
{
	logger.Log(sInfo.c_str());
}

namespace Global
{
	std::wstring GamePath;
}

PROCESS_INFORMATION _StartProcess_(PSTARTUP_INFO pStartInfo)
{
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	DWORD error = 0;
	TCHAR szDllPath[256] = { 0 };
	BOOL is64Process;
	TCHAR* szExe = pStartInfo->szExe;
	TCHAR* sPath = pStartInfo->sPath;

	if (!CreateProcess(szExe,
		pStartInfo->sCommandLine,
		NULL,
		NULL,
		NULL,
		0,
		NULL,
		NULL,
		&si,
		&pi
	))
	{
		error = GetLastError();
		logger.outDebug(_T("Hercules anti-cheat startup failed! (error:%d)"), error);
	}
	else
	{
		logger.outDebug(_T("Hercules anti-cheat started successfully.."));

	}
	return pi;
}
 
void CloseWindowsProcess(const std::wstring& wsProcess)
{
	std::vector<Common::ProcessInfo> processes = Common::EnumerateProcesses();
	if (processes.empty())
	{
		// Handle operation failure

	}
	else
	{
		// Handle the situation of successfully obtaining the process list
		for (const auto& process : processes)
		{
			if (_wcsicmp(process.processName.c_str(), wsProcess.c_str()) == 0)  // Case-insensitive comparison
			{
				if (process.processId != GetCurrentProcessId())
				{
					logger.outDebug(_T("Kill game process: %s"), process.processName.c_str());
					Common::TerminateWindowsProcess(process.processId);
				}
			}
		}
	}
}

// End the game client process
void ExitGameProcess()
{
	MessageBox(NULL, L"Found Hack Tools!", L"Hack Detected",0);
	CloseWindowsProcess(config[L"Client"]);
 }


PROCESS_INFORMATION StartProcess(std::wstring processPath, std::wstring procName, std::wstring sCommandLine)
{
	PROCESS_INFORMATION pi = { 0 };

	if (!processPath.empty())
	{
		std::wstring exePath = processPath + procName;
		STARTUP_INFO info = { 0 };
		wcscpy(info.szExe, exePath.c_str());
		wcscpy(info.sPath, processPath.c_str());
		wcscpy(info.sCommandLine, sCommandLine.c_str());
		pi = _StartProcess_(&info);
	}
	return pi;
}

// The specified program was not found
void NoProgramFound(std::wstring sText)
{
	::MessageBox(NULL, sText.c_str(), _T("HerculesAC Error:"), MB_ICONWARNING | MB_SYSTEMMODAL);
	exit(0);
}


void LoadConfig()
{
	VMProtectBeginVirtualization("VMP");

	// Traverse the game client directory
	std::vector<std::wstring> GameDir = FileSystem::TraverseDirectory(Global::GamePath);
	// Traverse the anti-cheat directory
	std::vector<std::wstring> CurDir = FileSystem::TraverseDirectory(FileSystem::GetModuleDirectory(NULL));

	std::wstring filename = FileSystem::GetModuleDirectory(NULL) + _T("hac.dat");
	logger.outDebug(_T("File path: %s"), filename.c_str());

	config[L"starter"] = FileSystem::ReadIniValue(filename, L"Game", L"starter");
	config[L"Client"] = FileSystem::ReadIniValue(filename, L"Game", L"Client");
	config[L"GameMon"] = FileSystem::ReadIniValue(filename, L"Game", L"GameMon");
	config[L"GameMon64"] = FileSystem::ReadIniValue(filename, L"Game", L"GameMon64");
 
	if (config[L"starter"].empty() ||
		config[L"Client"].empty() ||
		config[L"GameMon"].empty() ||
		config[L"GameMon64"].empty())
	{
		goto NoFound;
	}

	if (!FindProgram(GameDir, config[L"starter"]) ||
		!FindProgram(GameDir, config[L"Client"]) ||
		!FindProgram(CurDir, config[L"GameMon"]) ||
		!FindProgram(CurDir, config[L"GameMon64"]))
	{
		goto NoFound;
	}
	return;

NoFound:
	NoProgramFound(_T("The file is corrupted, please reinstall the game!"));
	VMProtectEnd();
}

// Check if Bac is already running
void CloseExistingHerculesAC()
{
	std::vector<Common::ProcessInfo> processes = Common::EnumerateProcesses();
	if (processes.empty())
	{
		// Handle operation failure

	}
	else
	{
		// Handle the situation of successfully obtaining the process list
		for (const auto& process : processes)
		{
			if (_wcsicmp(process.processName.c_str(), _T("HerculesAC.exe")) == 0)  // Case-insensitive comparison
			{
				if (process.processId != GetCurrentProcessId())
				{
					Common::TerminateWindowsProcess(process.processId);
				}
			}
		}
	}
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

void CheckProcessNames()
{
	VMProtectionScope vmpScope;

	if (!CheckGameClientIsAlive())
	{
		return;
	}

	if (std::any_of(detectedProcessNames.begin(), detectedProcessNames.end(), [](const auto& name) {
		if (Common::IsProcessRunning(name.c_str()))
		{
			ReportIllegalInfo("Found hack tools: " + Common::wideStringToString(name));
			return true;
		}
		else
		{
			return false;
		}
		}))
	{
		// Close the game client after detecting illegal programs
		ExitGameProcess();
	}
}

void CheckWindow()
{
	VMProtectionScope vmpScope;

	if (!CheckGameClientIsAlive())
	{
		return;
	}

	if (std::any_of(detectedWindowInfo.begin(), detectedWindowInfo.end(),
		[&](const std::pair<std::wstring, std::wstring>& info) {
			if (!info.first.empty())
			{
				if (Common::FindWindowInfo(info.first.c_str(), NULL))
				{
					std::wstring ClassName = L"Detected window class name: " + info.first;
					ReportIllegalInfo(Common::wideStringToString(ClassName));
					return true;
				}
			}

			if (!info.second.empty())
			{
				if (Common::FindWindowInfo(NULL, info.second.c_str()))
				{
					std::wstring titleName = L"Detected window title: " + info.second;
					ReportIllegalInfo(Common::wideStringToString(titleName));
					return true;
				}
			}
			return false;
		}))
	{
		// Close the game client after detecting illegal programs
		ExitGameProcess();
	}
}

/*
void CheckHandleName()
{
	HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, L"PVZCheatInstance");

	if ((hMutex != NULL))
	{
 		CloseHandle(hMutex);
		ExitGameProcess();
	}

}
*/
void DetectHackIsRunning()
{
	logger.outDebug(_T("Detect Hack Is Running"));
	CheckProcessNames();
	CheckWindow();
	// CheckHandleName();
}

void StartServer()
{
	for (;;)
	{
		if (!CheckGameClientIsAlive())
		{
			break;
		}
		DetectHackIsRunning();
		Sleep(5000);
	}
}

void UnInit()
{
	UnHook();
}

void InitProcess()
{
	
	PROCESS_INFORMATION pi = { 0 };

	std::wstring filename = FileSystem::GetModuleDirectory(NULL);
	if (!filename.empty())
	{
		pi = StartProcess(Global::GamePath, config[L"starter"], L"");

		Json::Value root;
		root["GamePath"] = Common::wideStringToString(Global::GamePath);
		root["pid"] = (UInt)pi.dwProcessId;
		root["hProcess"] = (UInt)pi.hProcess;
		Json::StreamWriterBuilder writerBuilder;
		std::string jsonString = Json::writeString(writerBuilder, root);
		StartProcess(filename, config[L"GameMon"], L" " + Common::stringToWideString(jsonString));
		StartProcess(filename, config[L"GameMon64"], L" " + Common::stringToWideString(jsonString));

	}
	
}


void InitHerculesAC()
{
	InitFunctionPointer();
	SetupHook();
	InitCheckSum();
	InitThread();
	InitProcess();

}

//Define the window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		std::wstring wsProcPath = FileSystem::GetModuleDirectory(NULL);
		std::wstring bannerImgPath = wsProcPath + L"assets\\hac.bmp";
		logger.outDebug(_T("banner path: %s"), bannerImgPath.c_str());
		// Load the image
		HBITMAP hBitmap = (HBITMAP)LoadImageW(NULL, bannerImgPath.data(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

		// Get the original size of the image
		BITMAP bitmap;
		GetObject(hBitmap, sizeof(BITMAP), &bitmap);
		int width = bitmap.bmWidth;
		int height = bitmap.bmHeight;
		int x = 0;
		int y = 0;

		// Draw the picture
		HDC memDC = CreateCompatibleDC(hdc);
		SelectObject(memDC, hBitmap);
		BitBlt(hdc, x, y, width, height, memDC, 0, 0, SRCCOPY);

		// Release resources
		DeleteDC(memDC);
		DeleteObject(hBitmap);

		EndPaint(hwnd, &ps);
		break;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DisplayBrand(
	_In_           HINSTANCE hInstance,
	_In_opt_       HINSTANCE hPrevInstance,
	_In_           LPSTR     lpCmdLine,
	_In_           int       nShowCmd
)
{
	// Register window class
	const wchar_t CLASS_NAME[] = L"DisplayBrandClass";

	// Register window class
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	//wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = CLASS_NAME;
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex)) {
		return 0;
	}

	std::wstring wsProcPath = FileSystem::GetModuleDirectory(NULL);
	std::wstring bannerImgPath = wsProcPath + L"assets\\hac.bmp";
	logger.outDebug(_T("banner path: %s"), bannerImgPath.c_str());
	// Load the image
	HBITMAP hBitmap = (HBITMAP)LoadImageW(NULL, bannerImgPath.data(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

	DWORD err = GetLastError();

	// Get the original size of the image
	BITMAP bitmap;
	GetObject(hBitmap, sizeof(BITMAP), &bitmap);
	int originalWidth = bitmap.bmWidth;
	int originalHeight = bitmap.bmHeight;
	DeleteObject(hBitmap);

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Scale or crop the image to fit the screen
	int width, height, x, y;
	if (originalWidth > screenWidth || originalHeight > screenHeight)
	{
		// The image size is larger than the screen size and needs to be scaled or cropped
		// Calculate the scaling ratio
		float scaleWidth = (float)screenWidth / originalWidth;
		float scaleHeight = (float)screenHeight / originalHeight;
		float scale = min(scaleWidth, scaleHeight);

		// Scale the image size
		width = (int)(originalWidth * scale);
		height = (int)(originalHeight * scale);

		// Calculate the center position of the screen
		x = (screenWidth - width) / 2;
		y = (screenHeight - height) / 2;
	}
	else
	{
		// The image size is less than or equal to the screen size, so it is directly displayed in the center
		width = originalWidth;
		height = originalHeight;
		x = (screenWidth - width) / 2;
		y = (screenHeight - height) / 2;
	}

	// Create a window
	HWND hwnd = CreateWindowEx(
		0,                              
		CLASS_NAME,                     
		L"",                
		WS_POPUP,                      
		x, y,                          
		width, height,                      
		NULL,                          
		NULL,                          
		hInstance,                     
		NULL                            
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	// Message loop
	MSG msg = { 0 };

	DWORD lastTick = GetTickCount();

	while (1)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// test if this is a quit
			if (msg.message == WM_QUIT)
				break;

			// translate any accelerator keys
			TranslateMessage(&msg);

			// send the message to the window proc
			DispatchMessage(&msg);

		} // end if

		if (GetTickCount() - lastTick > 5000)
		{
			DestroyWindow(hwnd);
		}

	} // end while
	return 0;
}

int CALLBACK WinMain(
	_In_           HINSTANCE hInstance,
	_In_opt_       HINSTANCE hPrevInstance,
	_In_           LPSTR     lpCmdLine,
	_In_           int       nShowCmd
)
{
	DisplayBrand(hInstance, hPrevInstance, lpCmdLine, nShowCmd);

	VMProtectBeginVirtualization("VMP");
	if (Common::SingletonPattern(L"HACMutexName"))
	{
		if (lpCmdLine && lpCmdLine[0] != '\0')
		{
			Global::GamePath = Common::stringToWideString(lpCmdLine);
			LoadConfig();
			InitHerculesAC();
			StartServer();
			UnInit();
		}
		Common::SingletonProgramEnd();
	}
	return 0;
	VMProtectEnd();
}