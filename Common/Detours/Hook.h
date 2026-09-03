#pragma once

#ifndef _HOOK_H
#define _HOOK_H

// Install the hook
void HookOn(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread);

// Uninstall hook
void HookOff(_In_ PVOID* pfun, _In_ PVOID proxy_fun, _In_ HANDLE hThread);


#endif // !_HOOK_H
