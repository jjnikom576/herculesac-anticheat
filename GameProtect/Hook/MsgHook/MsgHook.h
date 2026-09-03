#pragma once

#ifndef _MSG_HOOK_H
#define _MSG_HOOK_H

namespace MsgHook
{
	extern HHOOK hhk_mouse;

	extern "C" __declspec(dllexport) LRESULT CALLBACK GetMsgProc(
		_In_ int    code,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam
	);
	extern "C" __declspec(dllexport) void SetupMsgHook(HINSTANCE hinstDLL);
	extern "C" __declspec(dllexport) void UnHookMsgHook();
}


#endif // !_MSG_HOOK_H
