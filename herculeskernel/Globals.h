#pragma once

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "Protect/PidTable.h"

typedef struct _PROTECT_OBJECT_ENTRY
{
	DWORD dwProcessId;
	DWORD dwThreadId;

	LIST_ENTRY EventList;
} PROTECT_OBJECT_ENTRY, * PPROTECT_OBJECT_ENTRY;

typedef struct _PROTECT_OBJECT
{
	MY_LIST EventList;        
	FAST_MUTEX Mutex;            
} PROTECT_OBJECT, * PPROTECT_OBJECT;

extern PVOID g_obProcessHandle;
extern PVOID g_obThreadHandle;

extern hac::PidTable g_pidTable;
extern PDEVICE_OBJECT g_deviceObject;

// extern PROTECT_OBJECT g_ProtectObjectList;


 

#endif // !_GLOBALS_H
