#include "CRules.hpp"
#include "New.hpp"

constexpr ULONG RULE_MEM_TAG = 'mmuR';

NTSTATUS 
CRules::Initialized()
{
	NTSTATUS status{ STATUS_SUCCESS };

	m_tableOfTrustProcess		= new(NonPagedPoolNx) GenericTable<ProcessPath, _CompareProcessPath>;
	if (!m_tableOfTrustProcess)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	m_tableOfProtectProcess		= new(NonPagedPoolNx) GenericTable<ProcessPath, _CompareProcessPath>;
	if (!m_tableOfProtectProcess)
	{
		delete m_tableOfTrustProcess;
		m_tableOfTrustProcess = nullptr;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	m_tableOfProtectRegistry	= new(NonPagedPoolNx) GenericTable<RegistryPath, _CompareRegistryPath>;
	if (!m_tableOfProtectRegistry)
	{
		delete m_tableOfTrustProcess;
		m_tableOfTrustProcess = nullptr;

		delete m_tableOfProtectProcess;
		m_tableOfProtectProcess = nullptr;

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	return status;
}

VOID 
CRules::Finalized()
{
	if (m_tableOfTrustProcess)
	{
		delete m_tableOfTrustProcess;
		m_tableOfTrustProcess = nullptr;
	}

	if (m_tableOfProtectProcess)
	{
		delete m_tableOfProtectProcess;
		m_tableOfProtectProcess = nullptr;
	}

	if (m_tableOfProtectRegistry)
	{
		delete m_tableOfProtectRegistry;
		m_tableOfProtectRegistry = nullptr;
	}
}

NTSTATUS 
CRules::AddTrustProcess(IN CONST PWCHAR ProcessName)
{
	ProcessPath processPath{};
	if (sizeof(processPath) > wcslen(ProcessName) * sizeof(WCHAR))
	{
		RtlCopyMemory(processPath.Path, ProcessName, wcslen(ProcessName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(processPath.Path, ProcessName, sizeof(processPath) - sizeof(WCHAR));
	}

	
	return m_tableOfTrustProcess->AddElement(processPath);
}

NTSTATUS 
CRules::DelTrustProcess(IN CONST PWCHAR ProcessName)
{
	ProcessPath processPath{};
	if (sizeof(processPath) > wcslen(ProcessName) * sizeof(WCHAR))
	{
		RtlCopyMemory(processPath.Path, ProcessName, wcslen(ProcessName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(processPath.Path, ProcessName, sizeof(processPath) - sizeof(WCHAR));
	}
	return m_tableOfTrustProcess->DelElement(processPath);
}

BOOLEAN 
CRules::FindTrustProcess(IN CONST PWCHAR ProcessName)
{
	ProcessPath processPath{};
	if (sizeof(processPath) > wcslen(ProcessName) * sizeof(WCHAR))
	{
		RtlCopyMemory(processPath.Path, ProcessName, wcslen(ProcessName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(processPath.Path, ProcessName, sizeof(processPath) - sizeof(WCHAR));
	}

	return m_tableOfTrustProcess->IsInTable(processPath);
}

NTSTATUS 
CRules::AddProtectProcess(IN CONST PWCHAR ProcessName)
{
	ProcessPath processPath{};
	if (sizeof(processPath) > wcslen(ProcessName) * sizeof(WCHAR))
	{
		RtlCopyMemory(processPath.Path, ProcessName, wcslen(ProcessName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(processPath.Path, ProcessName, sizeof(processPath) - sizeof(WCHAR));
	}


	return m_tableOfProtectProcess->AddElement(processPath);
}

NTSTATUS 
CRules::DelProtectProcess(IN CONST PWCHAR ProcessName)
{
	ProcessPath processPath{};
	if (sizeof(processPath) > wcslen(ProcessName) * sizeof(WCHAR))
	{
		RtlCopyMemory(processPath.Path, ProcessName, wcslen(ProcessName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(processPath.Path, ProcessName, sizeof(processPath) - sizeof(WCHAR));
	}
	return m_tableOfProtectProcess->DelElement(processPath);
}

BOOLEAN 
CRules::FindProtectProcess(IN CONST PWCHAR ProcessName)
{
	ProcessPath processPath{};
	if (sizeof(processPath) > wcslen(ProcessName) * sizeof(WCHAR))
	{
		RtlCopyMemory(processPath.Path, ProcessName, wcslen(ProcessName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(processPath.Path, ProcessName, sizeof(processPath) - sizeof(WCHAR));
	}

	return m_tableOfProtectProcess->IsInTable(processPath);
}

NTSTATUS CRules::AddProtectRegistry(IN CONST PWCHAR RegistryName)
{
	RegistryPath registryPath{};
	if (sizeof(registryPath) > wcslen(RegistryName) * sizeof(WCHAR))
	{
		RtlCopyMemory(registryPath.Path, RegistryName, wcslen(RegistryName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(registryPath.Path, RegistryName, sizeof(registryPath) - sizeof(WCHAR));
	}

	return m_tableOfProtectRegistry->AddElement(registryPath);
}

NTSTATUS CRules::DelProtectRegistry(IN CONST PWCHAR RegistryName)
{
	RegistryPath registryPath{};
	if (sizeof(registryPath) > wcslen(RegistryName) * sizeof(WCHAR))
	{
		RtlCopyMemory(registryPath.Path, RegistryName, wcslen(RegistryName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(registryPath.Path, RegistryName, sizeof(registryPath) - sizeof(WCHAR));
	}

	return m_tableOfProtectRegistry->DelElement(registryPath);
}

BOOLEAN CRules::FindProtectRegistry(IN CONST PWCHAR RegistryName)
{
	RegistryPath registryPath{};
	if (sizeof(registryPath) > wcslen(RegistryName) * sizeof(WCHAR))
	{
		RtlCopyMemory(registryPath.Path, RegistryName, wcslen(RegistryName) * sizeof(WCHAR));
	}
	else
	{
		RtlCopyMemory(registryPath.Path, RegistryName, sizeof(registryPath) - sizeof(WCHAR));
	}

	return m_tableOfProtectRegistry->IsInTable(registryPath);
}
