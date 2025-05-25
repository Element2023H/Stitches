#include "RegistryProtector.hpp"
#include "CRules.hpp"
#include "Imports.hpp"
#include "Utils.hpp"
#include "Lazy.hpp"

extern LazyInstance<GlobalData> g_pGlobalData;

static 
BOOLEAN
AllowedRegistryOperation(
	IN CONST HANDLE Pid, 
	IN CONST PVOID RegObject)
{
	BOOLEAN bAllow = TRUE;

	BOOLEAN bTrustProcess	 = FALSE;
	BOOLEAN bProtectRegistry = FALSE;
	
	WCHAR wszRegistryPath[MAX_REGISTRYPATH]{ 0 };
	bProtectRegistry = KGetRegistryPath(RegObject, wszRegistryPath, MAX_REGISTRYPATH * sizeof(WCHAR));
	if (!bProtectRegistry)
	{
		bAllow = TRUE;
		return bAllow;
	}
	bProtectRegistry = CRULES_FIND_PROTECT_REGISTRY(wszRegistryPath);

	// trust process
	WCHAR wszProcessPath[MAX_PATH]{ 0 };
	auto status = GetProcessImageByPid(Pid, wszProcessPath);
	if (!NT_SUCCESS(status) &&
		!bProtectRegistry)
	{
		bAllow = TRUE;
		return bAllow;
	}
	bTrustProcess = CRULES_FIND_TRUST_PROCESS(wszProcessPath);

	if (bProtectRegistry && 
		!bTrustProcess)
	{
		bAllow = FALSE;
	}

	return bAllow;
}

RegistryProtectorEx::RegistryProtectorEx()
{

}

RegistryProtectorEx::~RegistryProtectorEx()
{
	if (!m_bSuccess)
	{
		return;
	}

	NTSTATUS status{ STATUS_SUCCESS };
	status = CmUnRegisterCallback(m_Cookie);
	if (NT_SUCCESS(status))
	{
		m_bSuccess = FALSE;
	}
}

NTSTATUS RegistryProtectorEx::Init()
{
	NTSTATUS status{ STATUS_SUCCESS };

	if (m_bSuccess)
	{
		return status;
	}

	if (0 == m_Cookie.QuadPart)
	{
		UNICODE_STRING usCallbackAltitude = {};
		RtlInitUnicodeString(&usCallbackAltitude, L"38325");

		status = CmRegisterCallbackEx(NotifyOnRegistryActions,
			&usCallbackAltitude,
			g_pGlobalData->pDriverObject,
			nullptr,
			&m_Cookie,
			nullptr);
		if (NT_SUCCESS(status))
		{
			m_bSuccess = TRUE;
		}
	}

	return status;
}

NTSTATUS 
RegistryProtectorEx::NotifyOnRegistryActions(
	_In_ PVOID CallbackContext, 
	_In_opt_ PVOID Argument1,
	_In_opt_ PVOID Argument2)
{
	UNREFERENCED_PARAMETER(CallbackContext);
	UNREFERENCED_PARAMETER(Argument1);
	UNREFERENCED_PARAMETER(Argument2);
	NTSTATUS status{ STATUS_SUCCESS };

	if (KeGetCurrentIrql() > PASSIVE_LEVEL)
	{
		return status;
	}

	auto eNotifyClass = static_cast<REG_NOTIFY_CLASS>((ULONG_PTR)Argument1);

	typedef struct _BASE_REG_KEY_INFO
	{
		PVOID		pObject;
		PVOID		reserved;
		// 
	} BASE_REG_KEY_INFO, * PBASE_REG_KEY_INFO;

	HANDLE	hPid = PsGetCurrentProcessId();
	BOOLEAN bAllowed = FALSE;

	switch (eNotifyClass)
	{

		//case RegNtPreOpenKey:
		//case RegNtPreOpenKeyEx:
		//
		//case RegNtPreCreateKey:
		//case RegNtPreCreateKeyEx:

	case RegNtPreDeleteKey:
	case RegNtPreRenameKey:
	case RegNtPreSetValueKey:
	case RegNtPreDeleteValueKey:
	{
		PBASE_REG_KEY_INFO pkeyInfo = reinterpret_cast<PBASE_REG_KEY_INFO>(Argument2);
		if (!pkeyInfo)
		{
			status = STATUS_SUCCESS;
			break;
		}

		bAllowed = AllowedRegistryOperation(hPid, pkeyInfo->pObject);
		if (!bAllowed)
		{
			status = STATUS_ACCESS_DENIED;
			break;
		}

	}
	break;

	default:
		break;
	}

	return status;
}
