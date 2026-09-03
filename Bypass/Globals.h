#ifndef _GLOBALS_H
#define _GLOBALS_H

extern PFN_FINDWINDOWW Sys_FindWindowW;
extern PFN_CREATETOOLHELP32SNAPSHOT Sys_CreateToolhelp32Snapshot;
extern "C" ULONG64 ptr_FindWindowA;
extern PFN_ADDRESS pfn_01_Crc32;
extern CRC32_t oCRC32;  // Pointer to the original function

#endif // !_GLOBALS_H