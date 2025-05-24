#include "Utils.hpp"


extern GlobalData* g_pGlobalData;

constexpr ULONG MEM_ALLOC_TAG		= 'htaP';
constexpr ULONG REGISTRY_MEM_TAG	= 'mtsR';

#ifndef SYSTEM_PROCESS_NAME
#define SYSTEM_PROCESS_NAME L"System"
#endif

#ifndef MAX_PROCESS_IMAGE_LENGTH
#define MAX_PROCESS_IMAGE_LENGTH	520
#endif

#ifndef MAX_REGISTRY_PATH_LENGTH
#define MAX_REGISTRY_PATH_LENGTH	(512 * sizeof(WCHAR))
#endif

WCHAR*
KWstrnstr(
	const WCHAR* src,
	const WCHAR* find)
{
	WCHAR* cp = (WCHAR*)src;
	WCHAR* s1 = NULL, * s2 = NULL;

	if (NULL == src ||
		NULL == find)
	{
		return NULL;
	}

	while (*cp)
	{
		s1 = cp;
		s2 = (WCHAR*)find;

		while (*s2 && *s1 && !(towlower(*s1) - towlower(*s2)))
		{
			s1++, s2++;
		}

		if (!(*s2))
		{
			return cp;
		}

		cp++;
	}
	return NULL;
}


PVOID
KGetProcAddress(
	IN CONST HANDLE ModuleHandle,
	CONST PCHAR FuncName)
{
	PIMAGE_DOS_HEADER       DosHeader = NULL;
	PIMAGE_NT_HEADERS       NtHeader = NULL;
	PIMAGE_DATA_DIRECTORY   ExportsDir = NULL;
	PIMAGE_EXPORT_DIRECTORY Exports = NULL;
#ifdef _WIN64
	PIMAGE_NT_HEADERS32		pNtHeaders32;
#endif
	PULONG Functions = NULL;
	PSHORT Ordinals = NULL;
	PULONG Names = NULL;
	ULONG64 ProcAddr = 0;
	ULONG NumOfFunc = 0;

	ULONG i = 0, NumOfNames = 0, iOrd = 0, nSize = 0, ulImageSize = 0;

	if (!ModuleHandle || !FuncName)
	{
		return NULL;
	}

	DosHeader = (PIMAGE_DOS_HEADER)ModuleHandle;

	NtHeader = (PIMAGE_NT_HEADERS)((PUCHAR)ModuleHandle + DosHeader->e_lfanew);

	if (!NtHeader)
	{
		return NULL;
	}
#ifdef _WIN64
	if (NtHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		ExportsDir = NtHeader->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_EXPORT;
		ulImageSize = NtHeader->OptionalHeader.SizeOfImage;
	}
	else
	{
		pNtHeaders32 = (PIMAGE_NT_HEADERS32)NtHeader;
		ExportsDir = pNtHeaders32->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_EXPORT;
		ulImageSize = NtHeader->OptionalHeader.SizeOfImage;
	}
#else
	ExportsDir = NtHeader->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_EXPORT;
	ulImageSize = NtHeader->OptionalHeader.SizeOfImage;
#endif 
	if (!ExportsDir)
	{
		return NULL;
	}

	Exports = (PIMAGE_EXPORT_DIRECTORY)((PUCHAR)ModuleHandle + ExportsDir->VirtualAddress);

	if (!Exports)
	{
		return NULL;
	}

	Functions = (PULONG)((PUCHAR)ModuleHandle + Exports->AddressOfFunctions);
	Ordinals = (PSHORT)((PUCHAR)ModuleHandle + Exports->AddressOfNameOrdinals);
	Names = (PULONG)((PUCHAR)ModuleHandle + Exports->AddressOfNames);

	NumOfNames = Exports->NumberOfNames;
	ProcAddr = ExportsDir->VirtualAddress;
	NumOfFunc = Exports->NumberOfFunctions;

	nSize = ExportsDir->Size;
	__try
	{
		for (i = 0; i < NumOfNames; i++)
		{
			iOrd = Ordinals[i];
			if (iOrd >= NumOfFunc)
			{
				continue;
			}

			if (Functions[iOrd] > 0 && Functions[iOrd] < ulImageSize)
			{
				if (_stricmp((char*)ModuleHandle + Names[i], FuncName) == 0)
				{
					return (PVOID)((char*)ModuleHandle + Functions[iOrd]);
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return NULL;
	}

	return NULL;
}

static
NTSTATUS
KQuerySymbolicLink(
	IN  PUNICODE_STRING SymbolicLinkName,
	OUT PWCHAR			SymbolicLinkTarget)
{
	if (!SymbolicLinkName)
	{
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS            status = STATUS_SUCCESS;
	HANDLE              hLink = NULL;
	OBJECT_ATTRIBUTES   oa{};
	UNICODE_STRING		LinkTarget{};
	// 这里也是醉了
	InitializeObjectAttributes(&oa, SymbolicLinkName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, 0, 0);

	// 通过对象先打开符号链接
	status = ZwOpenSymbolicLinkObject(&hLink, GENERIC_READ, &oa);
	if (!NT_SUCCESS(status) || !hLink)
	{
		return status;
	}

	// 申请内存
	LinkTarget.Length = MAX_PATH * sizeof(WCHAR);
	LinkTarget.MaximumLength = LinkTarget.Length + sizeof(WCHAR);
	LinkTarget.Buffer = (PWCH)ExAllocatePoolWithTag(NonPagedPool, LinkTarget.MaximumLength, MEM_ALLOC_TAG);
	if (!LinkTarget.Buffer)
	{
		ZwClose(hLink);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(LinkTarget.Buffer, LinkTarget.MaximumLength);

	// 获取符号链接名
	status = ZwQuerySymbolicLinkObject(hLink, &LinkTarget, NULL);
	if (NT_SUCCESS(status))
	{
		RtlCopyMemory(SymbolicLinkTarget, LinkTarget.Buffer, wcslen(LinkTarget.Buffer) * sizeof(WCHAR));
	}
	if (LinkTarget.Buffer)
	{
		ExFreePoolWithTag(LinkTarget.Buffer, MEM_ALLOC_TAG);
	}

	if (hLink)
	{
		ZwClose(hLink);
		hLink = nullptr;
	}


	return status;
}

// 设备路径转dos路径
// 原理是枚举从a到z盘的设备目录,然乎通过ZwOpenSymbolicLinkObject
// 来获取该设备对应的符号链接,匹配上的话,符号连接就是盘符
NTSTATUS
KGetDosProcessPath(
	IN	PWCHAR DeviceFileName,
	OUT PWCHAR DosFileName)
{
	NTSTATUS			status = STATUS_SUCCESS;
	WCHAR				DriveLetter{};
	WCHAR				DriveBuffer[30] = L"\\??\\C:";
	UNICODE_STRING		DriveLetterName{};
	WCHAR				LinkTarget[260]{};

	RtlInitUnicodeString(&DriveLetterName, DriveBuffer);

	DosFileName[0] = 0;

	// 从 a 到 z开始枚举 一个个尝试
	for (DriveLetter = L'A'; DriveLetter <= L'Z'; DriveLetter++)
	{
		// 替换盘符
		DriveLetterName.Buffer[4] = DriveLetter;

		// 通过设备名获取符号连接名
		status = KQuerySymbolicLink(&DriveLetterName, LinkTarget);
		if (!NT_SUCCESS(status))
		{
			continue;
		}

		// 判断设备是否与匹配,匹配上的话就是,进行拷贝即可
		if (_wcsnicmp(DeviceFileName, LinkTarget, wcslen(LinkTarget)) == 0)
		{
			wcscpy(DosFileName, DriveLetterName.Buffer + 4);
			wcscat(DosFileName, DeviceFileName + wcslen(LinkTarget));
			break;
		}
	}
	return status;
}

NTSTATUS
GetProcessImageByPid(
	IN CONST HANDLE Pid,
	IN OUT PWCHAR ProcessImage)
{
	NTSTATUS status = STATUS_SUCCESS;
	PEPROCESS pEprocess = NULL;
	HANDLE hProcess = NULL;
	PVOID pProcessPath = NULL;

	ULONG uProcessImagePathLength = 0;

	if (!ProcessImage || Pid < (ULongToHandle)(4))
	{
		return STATUS_INVALID_PARAMETER;
	}

	// 修复了bug
	if (Pid == (ULongToHandle)(4))
	{
		RtlCopyMemory(ProcessImage, SYSTEM_PROCESS_NAME, sizeof(SYSTEM_PROCESS_NAME));
		return status;
	}

	status = PsLookupProcessByProcessId(Pid, &pEprocess);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	__try
	{
		do
		{
			status = ObOpenObjectByPointer(pEprocess,
				OBJ_KERNEL_HANDLE,
				NULL,
				PROCESS_ALL_ACCESS,
				*PsProcessType,
				KernelMode,
				&hProcess);
			if (!NT_SUCCESS(status))
			{
				break;
			}
			//__TIME__

			// 获取长度
			// https://learn.microsoft.com/zh-cn/windows/win32/procthread/zwqueryinformationprocess
			status = ZwQueryInformationProcess(hProcess,
				ProcessImageFileName,
				NULL,
				0,
				&uProcessImagePathLength);
			if (STATUS_INFO_LENGTH_MISMATCH == status)
			{
				// 申请长度+sizeof(UNICODE_STRING)为了安全起见
				pProcessPath = ExAllocatePoolWithTag(NonPagedPool,
					uProcessImagePathLength + sizeof(UNICODE_STRING),
					MEM_ALLOC_TAG);
				if (pProcessPath)
				{
					RtlZeroMemory(pProcessPath, uProcessImagePathLength + sizeof(UNICODE_STRING));

					// 获取数据
					status = ZwQueryInformationProcess(hProcess,
						ProcessImageFileName,
						pProcessPath,
						uProcessImagePathLength,
						&uProcessImagePathLength);
					if (!NT_SUCCESS(status))
					{
						break;
					}

					status = KGetDosProcessPath(reinterpret_cast<PUNICODE_STRING>(pProcessPath)->Buffer, ProcessImage);
					if (!NT_SUCCESS(status))
					{
						break;
					}
				}
			}// end if (STATUS_INFO_LENGTH_MISMATCH == status)
		} while (FALSE);
	}
	__finally
	{

		if (pProcessPath)
		{
			ExFreePoolWithTag(pProcessPath, MEM_ALLOC_TAG);
			pProcessPath = NULL;
		}


		if (hProcess)
		{
			ZwClose(hProcess);
			hProcess = NULL;
		}
	}

	ObDereferenceObject(pEprocess);

	return status;
}


NTSTATUS
GetProcessImage(
	IN CONST PEPROCESS Process,
	IN OUT PWCHAR ProcessImage)
{
	if (!ProcessImage || !Process)
	{
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS status = STATUS_UNSUCCESSFUL;

	HANDLE hProcess = nullptr;

	ULONG uProcessImagePathLength = 0;
	PVOID pProcessPath = nullptr;
	
	__try
	{
		do
		{
			status = ObOpenObjectByPointer(Process,
				OBJ_KERNEL_HANDLE,
				nullptr,
				PROCESS_ALL_ACCESS,
				*PsProcessType,
				KernelMode,
				&hProcess);

			if (!NT_SUCCESS(status) || !hProcess)
			{
				break;
			}

			// 获取长度
			// https://learn.microsoft.com/zh-cn/windows/win32/procthread/zwqueryinformationprocess
			status = ZwQueryInformationProcess(hProcess,
				ProcessImageFileName,
				nullptr,
				0,
				&uProcessImagePathLength);
			if (STATUS_INFO_LENGTH_MISMATCH == status)
			{
				// 申请长度+sizeof(UNICODE_STRING)为了安全起见
				pProcessPath = ExAllocatePoolWithTag(NonPagedPool,
					uProcessImagePathLength + sizeof(UNICODE_STRING),
					MEM_ALLOC_TAG);
				if (pProcessPath)
				{
					RtlZeroMemory(pProcessPath, uProcessImagePathLength + sizeof(UNICODE_STRING));

					// 获取数据
					status = ZwQueryInformationProcess(hProcess,
						ProcessImageFileName,
						pProcessPath,
						uProcessImagePathLength,
						&uProcessImagePathLength);
					if (!NT_SUCCESS(status))
					{
						break;
					}

					status = KGetDosProcessPath(reinterpret_cast<PUNICODE_STRING>(pProcessPath)->Buffer, ProcessImage);				
					if (!NT_SUCCESS(status))
					{
						break;
					}

					//RtlCopyMemory(ProcessImage, pUstrProcessName->Buffer, pUstrProcessName->Length);
				}
			}// end if (STATUS_INFO_LENGTH_MISMATCH == status)
		} while (FALSE);
	}
	__finally
	{

		if (pProcessPath)
		{
			ExFreePoolWithTag(pProcessPath, MEM_ALLOC_TAG);
			pProcessPath = nullptr;
		}


		if (hProcess)
		{
			ZwClose(hProcess);
			hProcess = nullptr;
		}
	}

	// 严谨
	if (Process)
	{
		ObDereferenceObject(Process);
	}

	return status;
}


//************************************
// Method:    UnicodeStringContains
// FullName:  UnicodeStringContains
// Access:    public 
// Returns:   BOOLEAN
// Qualifier:
// Parameter: PUNICODE_STRING UnicodeString
// Parameter: PCWSTR SearchString
// https://github.com/Xacone/BestEdrOfTheMarket/blob/main/BestEdrOfTheMarketDriver/src/Utils.cpp
//************************************
BOOLEAN
UnicodeStringContains(
	PUNICODE_STRING UnicodeString,
	PCWSTR          SearchString)
{

	if (UnicodeString == NULL || 
		UnicodeString->Buffer == NULL || 
		SearchString == NULL)
	{
		return FALSE;
	}

	size_t searchStringLength = wcslen(SearchString);
	if (searchStringLength == 0)
	{
		return FALSE;
	}

	USHORT unicodeStringLengthInChars = UnicodeString->Length / sizeof(WCHAR);

	if (unicodeStringLengthInChars < searchStringLength)
	{
		return FALSE;
	}

	for (USHORT i = 0; i <= unicodeStringLengthInChars - searchStringLength; i++)
	{
		if (!MmIsAddressValid(&UnicodeString->Buffer[i]))
		{
			return FALSE;
		}

		if (wcsncmp(&UnicodeString->Buffer[i], SearchString, searchStringLength) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN 
IsProtectedProcess(IN CONST PEPROCESS Process)
{
	if (!Process)
	{
		return FALSE;
	}

	if (g_pGlobalData->PsIsProtectedProcess)
	{
		return (g_pGlobalData->PsIsProtectedProcess(Process) != 0);
	}
	else if (g_pGlobalData->PsIsProtectedProcessLight(Process))
	{
		return (g_pGlobalData->PsIsProtectedProcessLight(Process) != 0);
	}

	return FALSE;
}


_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
KTerminateProcess(IN CONST ULONG ProcessId)
{
	if (ProcessId <= 4 || 
		KeGetCurrentIrql > PASSIVE_LEVEL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	HANDLE hProcess = nullptr;
	OBJECT_ATTRIBUTES oa{};
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	if (!g_pGlobalData->ZwTerminateProcess)
	{
		UNICODE_STRING ustrZwTerminateProcess{};
		RtlInitUnicodeString(&ustrZwTerminateProcess, ZWTERMINATEPROCESS);

		// 再次获取下地址
		g_pGlobalData->ZwTerminateProcess = reinterpret_cast<PfnZwTerminateProcess>(
			MmGetSystemRoutineAddress(&ustrZwTerminateProcess));
		if (!g_pGlobalData->ZwTerminateProcess)
		{
			return STATUS_UNSUCCESSFUL;
		}
	}



	__try
	{
		oa.Length = sizeof(OBJECT_ATTRIBUTES);
		InitializeObjectAttributes(&oa, nullptr, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		CLIENT_ID clientId{};
		clientId.UniqueProcess = (ULongToHandle)(ProcessId);

		status = ZwOpenProcess(&hProcess, 1, &oa, &clientId);

		// 获取进程句柄
		if (NT_SUCCESS(status))
		{
			status = g_pGlobalData->ZwTerminateProcess(hProcess, STATUS_SUCCESS);
		}
	}
	__finally
	{
		if (hProcess)
		{
			ZwClose(hProcess);
			hProcess = nullptr;
		}
	}
	return status;
}

BOOLEAN 
KGetRegistryPath(
	IN PVOID RegistryObject, 
	IN OUT PWCHAR Buffer,
	IN CONST ULONG BufferSize)
{
	if (!RegistryObject ||
		!Buffer ||
		!BufferSize || 
		!MmIsAddressValid(RegistryObject))
	{
		return FALSE;
	}

	if (KeGetCurrentIrql() > PASSIVE_LEVEL ||
		PsGetCurrentProcessId() <= ULongToHandle(4))
	{
		return FALSE;
	}

	BOOLEAN bResult{ FALSE };

	ULONG nAllocSize = sizeof(UNICODE_STRING) + MAX_REGISTRY_PATH_LENGTH;
	PUNICODE_STRING pObjectName = reinterpret_cast<PUNICODE_STRING>(ExAllocatePoolWithTag(NonPagedPoolNx, nAllocSize, REGISTRY_MEM_TAG));
	if (pObjectName)
	{
		RtlZeroMemory(pObjectName, nAllocSize);
		pObjectName->Length = MAX_REGISTRY_PATH_LENGTH;
		pObjectName->MaximumLength = MAX_REGISTRY_PATH_LENGTH + sizeof(WCHAR);

		ULONG nReturnLength = 0;
		auto status = ObQueryNameString(RegistryObject,
										reinterpret_cast<POBJECT_NAME_INFORMATION>(pObjectName),
										nAllocSize, 
										&nReturnLength);
		if (NT_SUCCESS(status) && 
			BufferSize > nReturnLength)
		{
			status = RtlStringCbCopyUnicodeString(Buffer, nReturnLength, pObjectName);
			bResult = NT_SUCCESS(status);
		}

		ExFreePoolWithTag(pObjectName, REGISTRY_MEM_TAG);
		pObjectName = nullptr;
	}
	else
	{
		return bResult;
	}


	return bResult;
}
