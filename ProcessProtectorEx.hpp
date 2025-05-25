#pragma once
#include "Imports.hpp"
#include "Lazy.hpp"


class ProcessProtectorEx
{
public:
	ProcessProtectorEx();
	~ProcessProtectorEx();

	NTSTATUS Init();

protected:
	static
	OB_PREOP_CALLBACK_STATUS
		ProcessPreOperationCallback(
			PVOID RegistrationContext,
			POB_PRE_OPERATION_INFORMATION OperationInformation
		);

	static
	OB_PREOP_CALLBACK_STATUS
		ThreadPreOperationCallback(
			PVOID RegistrationContext,
			POB_PRE_OPERATION_INFORMATION OperationInformation);

private:
	// ObRegisterCallbacks
	HANDLE		m_hObRegisterCallbacks{ nullptr };
	BOOLEAN		m_bObjectRegisterCreated{ FALSE };
};

static LazyInstance<ProcessProtectorEx> ProcessProtector;

// Syntax sugar
//#define PROCESS_PROTECTOR()			(ProcessProtector::getInstance())
//#define PROCESS_PROTECTOR_INIT()	(ProcessProtector::getInstance()->InitializeObRegisterCallbacks())
//#define PROCESS_PROTECTOR_DESTROY() (ProcessProtector::getInstance()->FinalizeObRegisterCallbacks())