#ifndef _WIN_MAIN_H
#define _WIN_MAIN_H

#include <Windows.h>
#include <tchar.h>
#include <DbgHelp.h>
#include <psapi.h>
#include <shlwapi.h>
#include <string>
#include <TlHelp32.h>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <process.h>
#include <assert.h>
#include <map>
#include <sstream>
#include <mutex>
#include <strsafe.h>
#include <intrin.h>
#include <winternl.h>
#include "../Common/VMProtectSDK.h"
#include "../Common/vmp.h"
#include "../Common/Common.h"
#include "../Common/FileSystem/FileSystem.h"
#include "../Common/Logger/Logger.h"
#include "../Common/Hash/Crc32.h"
#include "./HookCallSet/functionSet.h"
#include "../Common/include/json/json.h"
#include "../Common/include/List/vectorExt.h"
#include "Threads/ActiveThread.h"
#include "Globals.h"


using UInt = unsigned int;

typedef struct _STARTUP_INFO
{
	TCHAR szExe[256];
	TCHAR sPath[256];
	TCHAR sCommandLine[256];
}STARTUP_INFO, * PSTARTUP_INFO;

PROCESS_INFORMATION StartProcess(std::wstring processPath, std::wstring procName, std::wstring sCommandLine);

BOOL CheckGameClientIsAlive();
void ExitGameProcess();

BOOL FindProgram(std::vector<std::wstring> files, std::wstring Program);

extern Logger logger;
void ReportIllegalInfo(std::string sInfo);

#endif // !_WIN_MAIN_H