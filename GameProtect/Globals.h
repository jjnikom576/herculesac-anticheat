#pragma once

#ifndef _GLOBALS_H
#define _GLOBALS_H

extern SharedMemory* g_sharedMemory;
extern PFN_READPROCESSMEMORY Sys_ReadProcessMemory;
extern PFN_WRITEPROCESSMEMORY Sys_WriteProcessMemory;

namespace Global
{
	extern GAME_CLIENT game;
	extern DWORD dwReadCount;  
	extern DWORD dwWriteCount;
}

#endif // !_GLOBALS_H