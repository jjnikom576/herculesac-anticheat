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
#include <fstream>
#include <sstream>
#include <mutex>
#include "../Common/Common.h"
#include "../Common/FileSystem/FileSystem.h"
#include "../Common/Logger/Logger.h"

// startup information
 typedef struct _STARTUP_INFO
{
	TCHAR szExe[256];
	TCHAR sPath[256];
	TCHAR sCommandLine[256];
}STARTUP_INFO, * PSTARTUP_INFO;

#endif // !_WIN_MAIN_H