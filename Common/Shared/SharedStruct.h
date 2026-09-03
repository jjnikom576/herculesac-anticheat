#pragma once

#ifndef _SHARED_STRUCT_H
#define _SHARED_STRUCT_H

#define SYMBOLICLINK ((TCHAR*)_T("\\\\.\\HerculesAC"))

#define PATH_SIZE 260

#define PTR64(x) ULONG64

typedef struct _USER_DATA
{
    ULONG Count;  
	ULONG uSize;  
	PTR64(PVOID) pUserData;  
}USER_DATA, * PUSER_DATA;

typedef struct _USER_FUNCTION_RECORD
{
    WCHAR programName[100];      
    WCHAR functionName[100];    
    ULONG_PTR params[16];        
    ULONG_PTR retAddr;           
    ULONG uPid;                  
    ULONG uTid;                 
}USER_FUNCTION_RECORD, * PUSER_FUNCTION_RECORD;

typedef struct _PROTECT_TABLE_ENTRY {
    DWORD dwProcessId;
    DWORD dwThreadId;
}PROTECT_TABLE_ENTRY, * PPROTECT_TABLE_ENTRY;

#pragma pack(push, 1)
typedef struct _CHECK_VT_TABLE_ENTRY {
    bool boCheckVt;
    DWORD number;  
}CHECK_VT_TABLE_ENTRY, * PCHECK_VT_TABLE_ENTRY;
#pragma pack(pop)

typedef struct _SYMBOLICLINK_TABLE_ENTRY {
    WCHAR Symboliclink[64];
}SYMBOLICLINK_TABLE_ENTRY, * PSYMBOLICLINK_TABLE_ENTRY;


#endif // !_SHARED_STRUCT_H

