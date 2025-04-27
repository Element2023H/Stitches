#include "Utils.hpp"

constexpr ULONG MEM_ALLOC_TAG = 'htaP';

#ifndef SYSTEM_PROCESS_NAME
#define SYSTEM_PROCESS_NAME L"System"
#endif

#ifndef MAX_PROCESS_IMAGE_LENGTH
#define MAX_PROCESS_IMAGE_LENGTH	520
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
	// ����Ҳ������
	InitializeObjectAttributes(&oa, SymbolicLinkName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, 0, 0);

	// ͨ�������ȴ򿪷�������
	status = ZwOpenSymbolicLinkObject(&hLink, GENERIC_READ, &oa);
	if (!NT_SUCCESS(status) || !hLink)
	{
		return status;
	}

	// �����ڴ�
	LinkTarget.Length = MAX_PATH * sizeof(WCHAR);
	LinkTarget.MaximumLength = LinkTarget.Length + sizeof(WCHAR);
	LinkTarget.Buffer = (PWCH)ExAllocatePoolWithTag(NonPagedPool, LinkTarget.MaximumLength, MEM_ALLOC_TAG);
	if (!LinkTarget.Buffer)
	{
		ZwClose(hLink);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(LinkTarget.Buffer, LinkTarget.MaximumLength);

	// ��ȡ����������
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

// �豸·��תdos·��
// ԭ����ö�ٴ�a��z�̵��豸Ŀ¼,Ȼ��ͨ��ZwOpenSymbolicLinkObject
// ����ȡ���豸��Ӧ�ķ�������,ƥ���ϵĻ�,�������Ӿ����̷�
static
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

	// �� a �� z��ʼö�� һ��������
	for (DriveLetter = L'A'; DriveLetter <= L'Z'; DriveLetter++)
	{
		// �滻�̷�
		DriveLetterName.Buffer[4] = DriveLetter;

		// ͨ���豸����ȡ����������
		status = KQuerySymbolicLink(&DriveLetterName, LinkTarget);
		if (!NT_SUCCESS(status))
		{
			continue;
		}

		// �ж��豸�Ƿ���ƥ��,ƥ���ϵĻ�����,���п�������
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

	// �޸���bug
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

			// ��ȡ����
			// https://learn.microsoft.com/zh-cn/windows/win32/procthread/zwqueryinformationprocess
			status = ZwQueryInformationProcess(hProcess,
				ProcessImageFileName,
				NULL,
				0,
				&uProcessImagePathLength);
			if (STATUS_INFO_LENGTH_MISMATCH == status)
			{
				// ���볤��+sizeof(UNICODE_STRING)Ϊ�˰�ȫ���
				pProcessPath = ExAllocatePoolWithTag(NonPagedPool,
					uProcessImagePathLength + sizeof(UNICODE_STRING),
					MEM_ALLOC_TAG);
				if (pProcessPath)
				{
					RtlZeroMemory(pProcessPath, uProcessImagePathLength + sizeof(UNICODE_STRING));

					// ��ȡ����
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