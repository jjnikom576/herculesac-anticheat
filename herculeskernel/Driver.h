#pragma once

#ifndef _DRIVER_H
#define _DRIVER_H

typedef unsigned char       BYTE;
typedef unsigned long       DWORD;


#include <ntifs.h>
#include <intrin.h>
#include <SymbolicAccess/ModuleExtender/ModuleExtenderFactory.h>
#include <SymbolicAccess/Utils/Log.h>
#include "../../Common/Shared/IOCTLs.h"
#include "../../Common/Shared/SharedStruct.h"
#include "../../Common/Ring0/Memory/AllocateMem.h"
#include "../../Common/Ring0/List/MyList.h"
#include "../../Common/Ring0/String/StringHandler.h"
#include "../../Common/Ring0/ia32-doc/out/ia32.hpp"
#include "../../Common/Ring0/Common.h"
#include "../../Common/Ring0/Encrypt/Blowfish/Blowfish.h"

#define PROCESS_TERMINATE         (0x0001)  // winnt
#define PROCESS_CREATE_THREAD     (0x0002)  // winnt
#define PROCESS_SET_SESSIONID     (0x0004)  // winnt
#define PROCESS_VM_OPERATION      (0x0008)  // winnt
#define PROCESS_VM_READ           (0x0010)  // winnt
#define PROCESS_VM_WRITE          (0x0020)  // winnt
// begin_ntddk begin_wdm begin_ntifs
#define PROCESS_DUP_HANDLE        (0x0040)  // winnt
// end_ntddk end_wdm end_ntifs
#define PROCESS_CREATE_PROCESS    (0x0080)  // winnt
#define PROCESS_SET_QUOTA         (0x0100)  // winnt
#define PROCESS_SET_INFORMATION   (0x0200)  // winnt
#define PROCESS_QUERY_INFORMATION (0x0400)  // winnt
#define PROCESS_SET_PORT          (0x0800)
#define PROCESS_SUSPEND_RESUME    (0x0800)  // winnt


typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    union {
        LIST_ENTRY HashLinks;
        struct {
            PVOID SectionPointer;
            ULONG CheckSum;
        };
    };
    union {
        ULONG TimeDateStamp;
        PVOID LoadedImports;
    };
    PVOID EntryPointActivationContext;
    PVOID PatchInformation;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT pDevice;
    UNICODE_STRING ustrDeviceName;	 
    UNICODE_STRING ustrSymLinkName; 
    PUCHAR buffer; 
    ULONG file_length; 
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;


 VOID InitProtect(IN PDRIVER_OBJECT pDriver_Object);
 VOID UninstallProtect();


#endif // !_DRIVER_H
