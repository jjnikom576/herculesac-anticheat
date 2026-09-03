#ifndef _GLOBALS_H
#define _GLOBALS_H


typedef __int64(__fastcall* PFN_BASETHREADINITTHUNK)(int a1, __int64(__fastcall* a2)(__int64), __int64 a3);

typedef VOID(NTAPI* PFN_RTLEXITUSERTHREAD)(_In_ NTSTATUS Status);

typedef void(WINAPI* PFN_ADDRESS)();

typedef HANDLE(WINAPI* PFN_CREATETHREAD)(_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ SIZE_T dwStackSize,
    _In_ LPTHREAD_START_ROUTINE lpStartAddress,
    _In_opt_ __drv_aliasesMem LPVOID lpParameter,
    _In_ DWORD dwCreationFlags,
    _Out_opt_ LPDWORD lpThreadId
    );

extern PFN_BASETHREADINITTHUNK Original_BaseThreadInitThunk;
extern PFN_BASETHREADINITTHUNK BaseThreadInitThunk;
extern PFN_RTLEXITUSERTHREAD RtlExitUserThread;
extern PFN_CREATETHREAD Sys_CreateThread;

#endif // !_GLOBALS_H