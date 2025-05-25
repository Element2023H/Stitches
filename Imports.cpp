#include "Imports.hpp"

GlobalData::GlobalData()
{
	fnZwQueryInformationProcess.Init(L"ZwQueryInformationProcess");
	fnZwQuerySystemInformation.Init(L"ZwQuerySystemInformation");
	fnCmCallbackGetKeyObjectIDEx.Init(L"CmCallbackGetKeyObjectIDEx");
	fnCmCallbackReleaseKeyObjectIDEx.Init(L"CmCallbackReleaseKeyObjectIDEx");

	PsIsProtectedProcess.Init(L"PsIsProtectedProcess");
	PsIsProtectedProcessLight.Init(L"PsIsProtectedProcessLight");
	ZwTerminateProcess.Init(L"ZwTerminateProcess");
	PsGetProcessWow64Process.Init(L"PsGetProcessWow64Process");

#if (NTDDI_VERSION >= NTDDI_WIN10_RS2)
	pfnPsSetCreateProcessNotifyRoutineEx2.Init(L"PsSetCreateProcessNotifyRoutineEx2");
#else
	pfnPsSetCreateProcessNotifyRoutineEx.Init(L"PsSetCreateProcessNotifyRoutineEx");
#endif
}

GlobalData::~GlobalData()
{
	if (InjectDllx64.Buffer)
	{
		ExFreePool(InjectDllx64.Buffer);
		InjectDllx64.Buffer = nullptr;
	}
	if (InjectDllx86.Buffer)
	{
		ExFreePool(InjectDllx86.Buffer);
		InjectDllx86.Buffer = nullptr;
	}
}
