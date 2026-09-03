#pragma once

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
#include <sstream>
#include <mutex>
#include <map>
#include <algorithm>
#include <winternl.h>
#include "../Common/Common.h"
#include "../Common/IPC/SharedMemory/SharedMemory.h"
#include "../Common/FileSystem/FileSystem.h"
#include "../Common/Logger/Logger.h"
#include "../Common/Manifest/Manifest.h"
#include "../Common/Manifest/PublicKey.h"

GAME_CLIENT GetSharedData();
extern "C" __declspec(dllexport) SharedMemory * GetSharedMemoryPtr();

#endif // !_DLLMAIN_H