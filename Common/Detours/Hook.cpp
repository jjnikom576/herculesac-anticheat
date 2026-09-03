#include <Windows.h>
#include "detours.h"
#include "Hook.h"


#ifdef _WIN64
#pragma comment(lib,"../Common/Detours/x64/detours.lib")
#else
#pragma comment(lib,"../Common/Detours/x86/detours.lib")
#endif // _WIN64

// Install the hook
void HookOn(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread)
{
	// Modify the target memory page protection attribute
	DetourTransactionBegin();
	// Pause the target thread
	DetourUpdateThread(hThread);
	// Start hook
	DetourAttach(pfun, proxy_fun);
	// Submit for execution
	DetourTransactionCommit();
}

// Uninstall hook
void HookOff(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread)
{
	// Modify the target memory page protection attribute
	DetourTransactionBegin();
	// Pause the target thread
	DetourUpdateThread(hThread);
	// Uninstall hook
	DetourDetach(pfun, proxy_fun);
	// Submit for execution
	DetourTransactionCommit();
}