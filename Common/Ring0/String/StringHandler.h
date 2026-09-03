#pragma once

#ifndef _STRING_HANDLER_H
#define _STRING_HANDLER_H

#define __T(x)      L ## x
#define _T(x)       __T(x)

SIZE_T GetStrLen(WCHAR* _Str);

VOID StrCopy(WCHAR* _Dest, WCHAR* _Source);
VOID StrCopy2(WCHAR* _Dest, PUNICODE_STRING _Source);

PVOID MemAllocate(IN SIZE_T Size, IN BOOLEAN Paged, IN ULONG Tag);

VOID FreeMemAllocate(IN PVOID Ptr, IN ULONG Tag);

BOOLEAN StrIsValid(WCHAR* _Str);

BOOLEAN StrIsValid2(UNICODE_STRING filePath);

PVOID GetModuleFileName(PUNICODE_STRING filePath);

BOOLEAN RtlUnicodeStringContains(PUNICODE_STRING Str, PUNICODE_STRING SubStr, BOOLEAN CaseInsensitive);

WCHAR* SplitString(WCHAR* SourceString, WCHAR* DestString, WCHAR Delimiter);

NTSTATUS GetProcessName(PEPROCESS Process, PUNICODE_STRING fileName);


#endif // !_STRING_HANDLER_H
