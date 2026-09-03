#include "../../dllmain.h"
#include "MsgHook.h"

//#pragma data_seg(".sdata")
//// Define shared data
//DWORD dd_value = 0;
//#pragma data_seg()
//
//#pragma comment(linker, "/section:.sdata,RWS")

namespace MsgHook
{
	HHOOK hhk_mouse;

	extern "C" __declspec(dllexport) LRESULT CALLBACK GetMsgProc(
		_In_ int    code,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam
	)
	{
		return CallNextHookEx(hhk_mouse, code, wParam, lParam);
	}

	extern "C" __declspec(dllexport) void SetupMsgHook(HINSTANCE hinstDLL)
	{
		hhk_mouse = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, hinstDLL, 0);
	}

	extern "C" __declspec(dllexport) void UnHookMsgHook()
	{
		UnhookWindowsHookEx(hhk_mouse);
	}
}