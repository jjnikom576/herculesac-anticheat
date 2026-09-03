#include "dllmain.h"
#include "../Common/IPC/SharedMemory/SharedMemory.h"
#include "Hook/DetoursHook/HookCallSet/functionSet.h"
#include "Globals.h"

SharedMemory* g_sharedMemory = nullptr;
PFN_READPROCESSMEMORY Sys_ReadProcessMemory;
PFN_WRITEPROCESSMEMORY Sys_WriteProcessMemory;


namespace Global
{
	GAME_CLIENT game;
	DWORD dwReadCount; 
	DWORD dwWriteCount;
}