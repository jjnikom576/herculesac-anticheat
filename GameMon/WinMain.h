#pragma once

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
#include <mutex>
#include <sstream>
#include "../Common/VMProtectSDK.h"
#include "../Common/vmp.h"
#include "../Common/Common.h"
#include "../Common/FileSystem/FileSystem.h"
#include "../Common/Logger/Logger.h"
#include "../Common/Hash/Crc32.h"
#include "../Common/IPC/SharedMemory/SharedMemory.h"
#include "../Common/include/json/json.h"

using UInt = unsigned int;

// Startup information
typedef struct _STARTUP_INFO
{
	TCHAR szExe[256];
	TCHAR sPath[256];
}STARTUP_INFO, * PSTARTUP_INFO;

typedef void(* PFN_SETUPMSGHOOK)(HINSTANCE hinstDLL);
typedef void(* PFN_UNHOOKMSGHOOK)();
typedef SharedMemory* (* PFN_GETSHAREDMEMORYPTR)();
typedef void(* PFN_ADDRESS)();

void ExitGameProcess();

// Check if the game client is alive
BOOL CheckGameClientIsAlive();

void InitFunctionPointer();

#endif // !_WIN_MAIN_H