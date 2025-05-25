#pragma once
#include "Lazy.hpp"

class RegistryProtectorEx
{
public:
	RegistryProtectorEx();
	~RegistryProtectorEx();

	NTSTATUS Init();

protected:
	static
		NTSTATUS
		NotifyOnRegistryActions(
			_In_ PVOID CallbackContext,
			_In_opt_ PVOID Argument1,
			_In_opt_ PVOID Argument2);

public:
	LARGE_INTEGER m_Cookie{};
	BOOLEAN m_bSuccess{ FALSE };
};

static LazyInstance<RegistryProtectorEx> RegProtector;