#pragma once
#include "Singleton.hpp"


class RegistryProtector : public Singleton<RegistryProtector>
{
public:
	NTSTATUS Initialized();
	VOID Finalized();

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

#define REGISTRY_PROTECTOR()			(RegistryProtector::getInstance())
#define REGISTRY_PROTECTOR_INIT()		(RegistryProtector::getInstance()->Initialized())
#define REGISTRY_PROTECTOR_DESTROY()	(RegistryProtector::getInstance()->Finalized())


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