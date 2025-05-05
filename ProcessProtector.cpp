﻿#include "ProcessProtector.hpp"
#include "Utils.hpp"
#include "Log.hpp"

extern GlobalData* g_pGlobalData;

#define PROCESS_TERMINATE                  (0x0001)  
#define PROCESS_CREATE_THREAD              (0x0002)  
#define PROCESS_SET_SESSIONID              (0x0004)  
#define PROCESS_VM_OPERATION               (0x0008)  
#define PROCESS_VM_READ                    (0x0010)  
#define PROCESS_VM_WRITE                   (0x0020)  
#define PROCESS_DUP_HANDLE                 (0x0040)  
#define PROCESS_CREATE_PROCESS             (0x0080)  
#define PROCESS_SET_QUOTA                  (0x0100)  
#define PROCESS_SET_INFORMATION            (0x0200)  
#define PROCESS_QUERY_INFORMATION          (0x0400)  
#define PROCESS_SUSPEND_RESUME             (0x0800)  
#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)  
#define PROCESS_SET_LIMITED_INFORMATION    (0x2000)  



OB_PREOP_CALLBACK_STATUS
ProcessProtector::ProcessPreOperationCallback(
	PVOID RegistrationContext,
	POB_PRE_OPERATION_INFORMATION OperationInformation
)
{
	UNREFERENCED_PARAMETER(RegistrationContext);

	// 没有开启进程保护
	if (KeGetCurrentIrql() > PASSIVE_LEVEL)
	{
		return OB_PREOP_SUCCESS;
	}

	// Skip if access is from kernel
	if (OperationInformation->KernelHandle)
	{
		return OB_PREOP_SUCCESS;
	}

	if (!(PEPROCESS)OperationInformation->Object)
	{
		return OB_PREOP_SUCCESS;
	}

	// 增加一些验证
	// 验证是否是进程类型
	if (*PsProcessType != OperationInformation->ObjectType)
	{
		return OB_PREOP_SUCCESS;
	}


	// Accessor
	auto hInitiatorPid = PsGetCurrentProcessId();

	if (hInitiatorPid <= ULongToHandle(4))
	{
		return OB_PREOP_SUCCESS;
	}

	// Target Object
	auto hTargetPid = PsGetProcessId((PEPROCESS)OperationInformation->Object);

	// Destination process
	HANDLE			hDstPid			= nullptr;
	ACCESS_MASK*	pDesiredAccess	= nullptr;
	ACCESS_MASK     originalAccess  = 0;
	if (OB_OPERATION_HANDLE_CREATE == OperationInformation->Operation)
	{
		hDstPid = hInitiatorPid;
		pDesiredAccess = &OperationInformation->Parameters->CreateHandleInformation.DesiredAccess;

		originalAccess = OperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess;
	}
	else if (OB_OPERATION_HANDLE_DUPLICATE == OperationInformation->Operation)
	{
		auto& pInfo		= OperationInformation->Parameters->DuplicateHandleInformation;
		hDstPid			= PsGetProcessId((PEPROCESS)pInfo.TargetProcess);
		pDesiredAccess	= &pInfo.DesiredAccess;
		originalAccess  = pInfo.OriginalDesiredAccess;
	}
	else
	{
		return OB_PREOP_SUCCESS;
	}

	// skip self
	if (hInitiatorPid == hTargetPid)
	{
		return OB_PREOP_SUCCESS;
	}


	PUNICODE_STRING pTargetProcessPath{ nullptr };
	PUNICODE_STRING pCurrentProcessPath{ nullptr };
	NTSTATUS status{ STATUS_SUCCESS };

	status = SeLocateProcessImageName((PEPROCESS)OperationInformation->Object, &pTargetProcessPath);
	if (!NT_SUCCESS(status) && !pTargetProcessPath)
	{
		return OB_PREOP_SUCCESS;
	}

	status = SeLocateProcessImageName(PsGetCurrentProcess(), &pCurrentProcessPath);
	if (!NT_SUCCESS(status) && !pCurrentProcessPath)
	{
		if (pTargetProcessPath)
		{
			ExFreePool(pTargetProcessPath);
			pTargetProcessPath = nullptr;
		}

		return OB_PREOP_SUCCESS;
	}

	// 针对对目标进程lsass.exe读内存的操作
	// 如果当前进程是非法进程对lsass.exe进程进行操作
	{
		if (UnicodeStringContains(pTargetProcessPath, L"lsass.exe"))
		{
			if (!IsProtectedProcess(PsGetCurrentProcess()))
			{
				if (FlagOn(originalAccess, PROCESS_VM_READ))
				{
					// 如果是非保护进程操作
					// 无耻的话可以结束非保护进程
					*pDesiredAccess |= PROCESS_TERMINATE;
					
					// 不建议在这里进行
					// 可能会遇到APC_LEVEL无法执行Zw*(BSOD)
					// 考虑ProcessNotify过滤
					KTerminateProcess(HandleToULong(PsGetCurrentProcessId()));

					LOGINFO("[Read] %ws read lsass\r\n", pCurrentProcessPath->Buffer);
				}
			}
		}
	}

	if (FlagOn(originalAccess, PROCESS_TERMINATE))
	{
		// 如果是保护进程(业务设定)
		/*if (IsProtectProcess((PEPROCESS)OperationInformation->Object))
		{
			*pDesiredAccess &= ~PROCESS_TERMINATE;
		}*/

		// testing
		if (UnicodeStringContains(pTargetProcessPath, L"notepad.exe"))
		{
			*pDesiredAccess &= ~PROCESS_TERMINATE;

			LOGINFO("[Protected] %ws kill %ws\r\n ", pCurrentProcessPath->Buffer, pTargetProcessPath->Buffer);
		}
		
	}


	if (pTargetProcessPath)
	{
		ExFreePool(pTargetProcessPath);
		pTargetProcessPath = nullptr;
	}

	if (pCurrentProcessPath)
	{
		ExFreePool(pCurrentProcessPath);
		pCurrentProcessPath = nullptr;
	}

	// 

	/*
	* TODO...
	*/

	return OB_PREOP_SUCCESS;
}


OB_PREOP_CALLBACK_STATUS
ProcessProtector::ThreadPreOperationCallback(
	PVOID RegistrationContext,
	POB_PRE_OPERATION_INFORMATION OperationInformation
)
{
	UNREFERENCED_PARAMETER(RegistrationContext);

	if (!MmIsAddressValid(OperationInformation->Object))
	{
		return OB_PREOP_SUCCESS;
	}

	if (OperationInformation->Operation != OB_OPERATION_HANDLE_CREATE)
	{
		return OB_PREOP_SUCCESS;
	}

	// 换一种写法
	if (ExGetPreviousMode() == KernelMode)
	{
		return OB_PREOP_SUCCESS;
	}

	// Accessor
	auto hInitiatorPid = PsGetCurrentProcessId();

	if (hInitiatorPid <= ULongToHandle(4))
	{
		return OB_PREOP_SUCCESS;
	}


	// Target Object
	auto			hTargetPid		= PsGetProcessId((PEPROCESS)OperationInformation->Object);

	// Destination process
	HANDLE			hDstPid			= nullptr;

	ACCESS_MASK*	pDesiredAccess	= nullptr;

	if (OB_OPERATION_HANDLE_CREATE == OperationInformation->Operation)
	{
		hDstPid = hInitiatorPid;
		pDesiredAccess = &OperationInformation->Parameters->CreateHandleInformation.DesiredAccess;
	}
	else if (OB_OPERATION_HANDLE_DUPLICATE == OperationInformation->Operation)
	{
		auto& pInfo = OperationInformation->Parameters->DuplicateHandleInformation;
		hDstPid = PsGetProcessId((PEPROCESS)pInfo.TargetProcess);
		pDesiredAccess = &pInfo.DesiredAccess;
	}
	else
	{
		return OB_PREOP_SUCCESS;
	}

	// skip self
	if (hInitiatorPid == hTargetPid)
	{
		return OB_PREOP_SUCCESS;
	}


	if (FlagOn(*pDesiredAccess, THREAD_TERMINATE))
	{
		/*if (IsProtectProcess((PEPROCESS)OperationInformation->Object))
		{
			*pDesiredAccess &= ~THREAD_TERMINATE;
		}*/
	}

	return OB_PREOP_SUCCESS;

}



NTSTATUS 
ProcessProtector::InitializeObRegisterCallbacks()
{
	NTSTATUS status{ STATUS_UNSUCCESSFUL };

	OB_OPERATION_REGISTRATION stObOpReg[2] = {};
	OB_CALLBACK_REGISTRATION stObCbReg = {};

	USHORT OperationRegistrationCount = 0;

	do
	{
		if (m_bObjectRegisterCreated)
		{
			status = STATUS_SUCCESS;
			break;
		}

		// Processes callbacks
		stObOpReg[OperationRegistrationCount].ObjectType = PsProcessType;
		stObOpReg[OperationRegistrationCount].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
		stObOpReg[OperationRegistrationCount].PreOperation = ProcessPreOperationCallback;	// 
		OperationRegistrationCount += 1;

		stObOpReg[OperationRegistrationCount].ObjectType = PsThreadType;
		stObOpReg[OperationRegistrationCount].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
		stObOpReg[OperationRegistrationCount].PreOperation = ThreadPreOperationCallback;

		stObCbReg.Version = OB_FLT_REGISTRATION_VERSION;
		stObCbReg.OperationRegistrationCount = OperationRegistrationCount;
		stObCbReg.OperationRegistration = stObOpReg;
		RtlInitUnicodeString(&stObCbReg.Altitude, L"1000");

		status = ObRegisterCallbacks(&stObCbReg, &m_hObRegisterCallbacks);
		if (NT_SUCCESS(status))
		{
			m_bObjectRegisterCreated = TRUE;
			LOGINFO("bObjectRegisterCreated create success\r\n");
		}

	} while (FALSE);


	return status;
}

VOID 
ProcessProtector::FinalizeObRegisterCallbacks()
{
	if (!m_bObjectRegisterCreated)
	{
		return;
	}

	if (m_hObRegisterCallbacks)
	{
		ObUnRegisterCallbacks(m_hObRegisterCallbacks);
		m_hObRegisterCallbacks = nullptr;
	}

	m_bObjectRegisterCreated = FALSE;
}
