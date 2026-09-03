#include "dllmain.h"
#include "HookCallSet/functionSet.h"
#include "Globals.h"

PFN_FINDWINDOWW Sys_FindWindowW;
PFN_CREATETOOLHELP32SNAPSHOT Sys_CreateToolhelp32Snapshot;
ULONG64 ptr_FindWindowA;
PFN_ADDRESS pfn_01_Crc32;
CRC32_t oCRC32;  // Pointer to the original function