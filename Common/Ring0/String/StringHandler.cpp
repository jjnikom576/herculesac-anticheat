#include <ntifs.h>
#include "StringHandler.h"

SIZE_T GetStrLen(WCHAR* _Str)
{
    SIZE_T str_len = 0;
    if (_Str == NULL)
    {
        return 0;
    }

    for (;;)
    {
        if (*_Str == _T('\0'))
        {
            break;
        }
        _Str++;
        str_len++;
    }
    return str_len;
}

VOID StrCopy(WCHAR* _Dest, WCHAR* _Source)
{
    RtlCopyMemory(_Dest, _Source, GetStrLen(_Source) * sizeof(WCHAR));
}

VOID StrCopy2(WCHAR* _Dest, PUNICODE_STRING _Source)
{
    RtlCopyMemory(_Dest, _Source->Buffer, _Source->Length);
}

PVOID MemAllocate(IN SIZE_T Size, IN BOOLEAN Paged, IN ULONG Tag)
{
    PVOID pAddr = ExAllocatePoolWithTag(Paged ? PagedPool : NonPagedPool,
        Size,
        Tag);
    RtlZeroMemory(pAddr, Size);
    return pAddr;
}

VOID FreeMemAllocate(IN PVOID Ptr, IN ULONG Tag)
{
    ExFreePoolWithTag(Ptr, Tag);
}

BOOLEAN StrIsValid(WCHAR* _Str)
{
    if (*_Str == _T('\0'))
        return FALSE;
    else
        return TRUE;
}

BOOLEAN StrIsValid2(UNICODE_STRING filePath)
{
    if (filePath.Length == 0)
        return FALSE;
    else
        return TRUE;
}

PVOID GetModuleFileName(PUNICODE_STRING filePath)
{
    if (StrIsValid2(*filePath))
    {
        int i = filePath->Length / sizeof(WCHAR);

        while (filePath->Buffer[i] != L'\\')
        {
            i--;
        }

        return &filePath->Buffer[i + 1];
    }
    else
    {
        return NULL;
    }
}

NTSTATUS GetProcessName(PEPROCESS Process, PUNICODE_STRING fileName)
{
    NTSTATUS Status;
    PUNICODE_STRING ImageFileName;
    Status = SeLocateProcessImageName(Process, &ImageFileName);
    if (NT_SUCCESS(Status))
    {
        WCHAR* SubStr = (WCHAR*)GetModuleFileName(ImageFileName);
        if (StrIsValid(SubStr))
        {
            if (fileName)
            {
                RtlInitUnicodeString(fileName, SubStr);
            }            
        }
        if (ImageFileName)
        {
            ExFreePool(ImageFileName);
        }
    }
    return Status;
}

BOOLEAN RtlUnicodeStringContains(PUNICODE_STRING Str, PUNICODE_STRING SubStr, BOOLEAN CaseInsensitive)
{
    if (Str == NULL || SubStr == NULL || Str->Length < SubStr->Length)
        return FALSE;

    CONST USHORT NumCharsDiff = (Str->Length - SubStr->Length) / sizeof(WCHAR);
    UNICODE_STRING Slice = *Str;
    Slice.Length = SubStr->Length;

    for (USHORT i = 0; i <= NumCharsDiff; ++i, ++Slice.Buffer, Slice.MaximumLength -= sizeof(WCHAR))
    {
        if (RtlEqualUnicodeString(&Slice, SubStr, CaseInsensitive))
        {
            return TRUE;
        }
    }
    return FALSE;
}

WCHAR* SplitString(WCHAR* SourceString, WCHAR* DestString, WCHAR Delimiter)
{
    //WCHAR Buffer[1024] = { 0 };
    WCHAR* CurrentChar = SourceString;

    //StrCopy(Buffer, SourceString);

    if (SourceString == NULL || DestString == NULL)
        return NULL;

    for (ULONG i = 0; i < GetStrLen(SourceString); i++)
    {
        if (*CurrentChar == Delimiter)
        {
            *CurrentChar = _T('\0');
            StrCopy(DestString, SourceString);
            break;
        }

        CurrentChar++;
    }
    ++CurrentChar;


    if (*CurrentChar == _T('\0'))
        return NULL;

    return CurrentChar;
}
