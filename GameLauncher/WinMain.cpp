#include "WinMain.h"

Logger logger;

void _StartProcess_(PSTARTUP_INFO pStartInfo)
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
}


void StartProcess(std::wstring processPath, std::wstring procName, std::wstring sCommandLine)
{
	if (!processPath.empty())
	{
		std::wstring exePath = processPath + procName;
		STARTUP_INFO info = { 0 };
		wcscpy(info.szExe, exePath.c_str());
		wcscpy(info.sPath, processPath.c_str());
		wcscpy(info.sCommandLine, sCommandLine.c_str());
		_StartProcess_(&info);
	}
}

void InitHercules()
{
	std::wstring wsProcPath = FileSystem::GetModuleDirectory(NULL);

	if (!wsProcPath.empty())
	{
		StartProcess(wsProcPath, L"HerculesAC\\HerculesAC.aes", L" " + wsProcPath);
	}
}

int CALLBACK WinMain(
	_In_           HINSTANCE hInstance,
	_In_opt_       HINSTANCE hPrevInstance,
	_In_           LPSTR     lpCmdLine,
	_In_           int       nShowCmd
)
{

	InitHercules();
 	return 0;
}