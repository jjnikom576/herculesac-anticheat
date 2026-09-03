
#ifndef _FUNCTION_SET_H
#define _FUNCTION_SET_H

typedef DWORD (WINAPI* CRC32_t)(PVOID first_ptr, DWORD Size);


typedef HWND(WINAPI* PFN_FINDWINDOWW)(
    _In_opt_ LPCWSTR lpClassName,
    _In_opt_ LPCWSTR lpWindowName);

typedef HANDLE(WINAPI* PFN_CREATETOOLHELP32SNAPSHOT)(
    DWORD dwFlags,
    DWORD th32ProcessID
    );

typedef void(WINAPI* PFN_ADDRESS)();

void Hook_FindWindowW();
void UnHook_FindWindowW();

void Hook_01_Crc32();
void UnHook_01_Crc32();

void Hook_02_Crc32();
void UnHook_02_Crc32();

void Hook_CreateToolhelp32Snapshot();
void UnHook_CreateToolhelp32Snapshot();

#endif // !_FUNCTION_SET_H