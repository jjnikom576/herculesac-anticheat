#pragma once

#ifndef _FUNCTION_SET_H
#define _FUNCTION_SET_H

typedef BOOL(WINAPI* PFN_READPROCESSMEMORY)(
    __in      HANDLE hProcess,
    __in      LPCVOID lpBaseAddress,
    __out_bcount_part(nSize, *lpNumberOfBytesRead) LPVOID lpBuffer,
    __in      SIZE_T nSize,
    __out_opt SIZE_T* lpNumberOfBytesRead
    );

typedef BOOL(WINAPI* PFN_WRITEPROCESSMEMORY)(
    HANDLE  hProcess,
    LPVOID  lpBaseAddress,
    LPCVOID lpBuffer,
    SIZE_T  nSize,
    SIZE_T* lpNumberOfBytesWritten
    );

typedef void(WINAPI* PFN_ADDRESS)();


void Hook_ReadProcessMemory();
void UnHook_ReadProcessMemory();

void Hook_WriteProcessMemory();
void UnHook_WriteProcessMemory();


#endif // !_FUNCTION_SET_H