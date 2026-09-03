#include "../dllmain.h"
#include "ActiveThread.h"
#include "../Hook/DetoursHook/HookCallSet/functionSet.h"
#include "../Globals.h"
#include "../../Common/Hash/MD5/MD5.h"



unsigned __stdcall WorkerThread(PVOID pArgList)
{
	
	return 0;
}

void InitThread()
{
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
	CloseHandle(hThread);
}

void UnInitThread()
{

}