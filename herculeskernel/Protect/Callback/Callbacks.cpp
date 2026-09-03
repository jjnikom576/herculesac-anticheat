#include "../../Driver.h"
#include "Callbacks.h"
#include "../../Globals.h"

 
#define _Altitude_ L"321000"

//
HANDLE GetProcessIdByProcessObject(PEPROCESS processObject)
{
	return PsGetProcessId(processObject);
}

HANDLE GetProcessIdByThreadObject(PETHREAD pThread)
{
	PEPROCESS pProcess = PsGetThreadProcess(pThread);
	return PsGetProcessId(pProcess);
}

 
VOID PrintProcessName(PEPROCESS processObject)
{
	symbolic_access::ModuleExtenderFactory extenderFactory{};
	const auto& moduleExtender = extenderFactory.Create(L"ntoskrnl.exe");
	if (!moduleExtender.has_value())
		return;

	outLog("ProcessName: %s", moduleExtender->GetPointer<char>("_EPROCESS", "ImageFileName", processObject));
}


OB_PREOP_CALLBACK_STATUS preThreadCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	if (g_pidTable.Contains(static_cast<ULONG>((ULONG_PTR)GetProcessIdByThreadObject((PETHREAD)pOperationInformation->Object))))
	{

		if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)  
		{
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & THREAD_SUSPEND_RESUME) == THREAD_SUSPEND_RESUME)
			{
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~THREAD_SUSPEND_RESUME;
			}
		}
		else if (pOperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE)  
		{
			if ((pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess & THREAD_SUSPEND_RESUME) == THREAD_SUSPEND_RESUME)
			{
				pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~THREAD_SUSPEND_RESUME;
			}
		}
	}
	return OB_PREOP_SUCCESS;
}


OB_PREOP_CALLBACK_STATUS preProcessCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	if (g_pidTable.Contains(static_cast<ULONG>((ULONG_PTR)GetProcessIdByProcessObject((PEPROCESS)pOperationInformation->Object))))
	{
		
		if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE) 
		{
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_READ) == PROCESS_VM_READ)
			{
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
			}
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
			{
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
			}
		}
		else if (pOperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE)  
		{
			if ((pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess & PROCESS_VM_READ) == PROCESS_VM_READ)
			{
				pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
			}
			if ((pOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
			{
				pOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
			}
		}
	}
	return OB_PREOP_SUCCESS;
}

VOID SetThreadCallbacks(IN PDRIVER_OBJECT pDriver_Object)
{
	NTSTATUS Status;
	OB_OPERATION_REGISTRATION oor;
	OB_CALLBACK_REGISTRATION ocr;

	PLDR_DATA_TABLE_ENTRY ldr;
	ldr = (PLDR_DATA_TABLE_ENTRY)pDriver_Object->DriverSection;
	ldr->Flags |= 0x20;// This value will be checked when loading the driver. It must have a special signature, just add 0x20. Otherwise the call will fail

	oor.ObjectType = PsThreadType;
	oor.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	oor.PreOperation = (POB_PRE_OPERATION_CALLBACK)preThreadCallback;
	oor.PostOperation = NULL;

	ocr.Version = OB_FLT_REGISTRATION_VERSION;
	ocr.OperationRegistrationCount = 1;
	ocr.OperationRegistration = &oor;
	RtlInitUnicodeString(&ocr.Altitude, _Altitude_);
	ocr.RegistrationContext = NULL;

	Status = ObRegisterCallbacks(&ocr, &g_obThreadHandle);
	if (!NT_SUCCESS(Status))
	{
		ASSERT(FALSE);
	}
}

 VOID SetProcessCallbacks(IN PDRIVER_OBJECT pDriver_Object)
{
	NTSTATUS Status;
	OB_OPERATION_REGISTRATION oor;
	OB_CALLBACK_REGISTRATION ocr;

	PLDR_DATA_TABLE_ENTRY ldr;
	ldr = (PLDR_DATA_TABLE_ENTRY)pDriver_Object->DriverSection;
	ldr->Flags |= 0x20;// This value will be checked when loading the driver. It must have a special signature, just add 0x20. Otherwise the call will fail

	oor.ObjectType = PsProcessType;
	oor.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	oor.PreOperation = (POB_PRE_OPERATION_CALLBACK)preProcessCallback;
	oor.PostOperation = NULL;

	ocr.Version = OB_FLT_REGISTRATION_VERSION;
	ocr.OperationRegistrationCount = 1;
	ocr.OperationRegistration = &oor;
	RtlInitUnicodeString(&ocr.Altitude, _Altitude_);
	ocr.RegistrationContext = NULL;

	Status = ObRegisterCallbacks(&ocr, &g_obProcessHandle);
	if (!NT_SUCCESS(Status))
	{
		ASSERT(FALSE);
	}
}

 VOID UnThreadCallbacks()
{
	ASSERT(g_obThreadHandle);
	if (g_obThreadHandle)
	{
		ObUnRegisterCallbacks(g_obThreadHandle);
	}
}

 VOID UnProcessCallbacks()
{
	ASSERT(g_obProcessHandle);
	if (g_obProcessHandle)
	{
		ObUnRegisterCallbacks(g_obProcessHandle);
	}
}

static VOID OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
	UNREFERENCED_PARAMETER(ThreadId);
	if (!Create)
		return;
	ULONG pid = static_cast<ULONG>((ULONG_PTR)ProcessId);
	if (g_pidTable.Contains(pid)) {
		outLog("ThreadNotify: new thread in protected PID %lu", pid);
	}
}

VOID InitThreadNotify()
{
	NTSTATUS status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
	if (!NT_SUCCESS(status)) {
		outLog("PsSetCreateThreadNotifyRoutine failed: 0x%08X", status);
	}
}

VOID UninstallThreadNotify()
{
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
}