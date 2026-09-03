#ifndef _DLLMAIN_H
#define _DLLMAIN_H

#include <Windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <process.h>
#include <fstream>
#include <string>
#include <ctime>
#include <TlHelp32.h>
#include <assert.h>
#include "../Common/Common.h"

void SetupHook();

void UnHook();

#endif // !_DLLMAIN_H
