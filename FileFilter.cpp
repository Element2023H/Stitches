#include <initguid.h>
#include "FileFilter.hpp"
#include "Log.hpp"
#include "New.hpp"
#include "Notify.hpp"
#include "ProcessCtx.hpp"
#include "ProcessProtectorEx.hpp"
#include "DeviceControl.hpp"
#include "Common.h"
#include "CRules.hpp"
#include "Utils.hpp"
#include "Lazy.hpp"
#include "RegistryProtector.hpp"

extern LazyInstance<GlobalData> g_pGlobalData;
extern HANDLE g_hFile;

static
NTSTATUS 
UnloadFilter(IN FLT_FILTER_UNLOAD_FLAGS Flags);

NTSTATUS
UnloadFilter(IN FLT_FILTER_UNLOAD_FLAGS Flags)
{
	UNREFERENCED_PARAMETER(Flags);

	NOTIFY_DESTROY();
	delete NOTIFY();

	LazyInstance<RegistryProtectorEx>::Dispose();

	LazyInstance<ProcessProtectorEx>::Dispose();

	FILEFILTER_DESTROY();
	delete FILEFILTER();

	PROCESS_CTX_CLEAR();
	ExDeleteNPagedLookasideList(&g_pGlobalData->ProcessCtxNPList);
	delete PROCESS_CTX_INSTANCE();

	UNICODE_STRING ustrDeviceName{};
	RtlInitUnicodeString(&ustrDeviceName, DEVICE_NAME);
	UNICODE_STRING ustrSymbolicLink{};
	RtlInitUnicodeString(&ustrSymbolicLink, SYMBOLICLINK_NAME);
	DEVICE_CTL_FINALIZED(&ustrDeviceName, &ustrSymbolicLink);
	delete DEVICE_CTL_INSTANCE();

	CRULES_DESTROY();
	delete CRULES_INSTANCE();

	if (g_hFile)
	{
		ZwClose(g_hFile);
		g_hFile = nullptr;
	}

	LazyInstance<GlobalData>::Dispose();

	return STATUS_SUCCESS;
}

struct InstanceContext
{
	ULONG Tag;
	ULONG SectorSize;
	FLT_FILESYSTEM_TYPE VolumeFilesystemType;
	ULONG DeviceCharacteristics;
};

constexpr ULONG INSTANCE_CONTEXT_SIZE = sizeof(InstanceContext);
constexpr ULONG INSTANCE_CONTEXT_TAG = 'XTCI';
constexpr ULONG INSTANCE_MEM_TAG = 'xtcI';

//// ESP of Windows  defender
#ifndef GUID_ECP_MSSECFLT_OPEN_DEFINED

DEFINE_GUID(GUID_ECP_MSSECFLT_OPEN, 0xAB97C9D8, 0x9A82, 0x4E58, 0xA2, 0x09,
	0xCD, 0x56, 0xC5, 0x8A, 0xA5, 0xD4);

#endif // GUID_ECP_MSSECFLT_OPEN_DEFINED
//
//// This GUID is used to identify ECP that is sent by CsvFs to the
//// Metadata Node (MDS a.k.a. Coordinating Node), and contains information
//// about the type of the create.
#if (NTDDI_VERSION < NTDDI_WIN8)
DEFINE_GUID(GUID_ECP_CSV_DOWN_LEVEL_OPEN,
	0x4248be44,
	0x647f,
	0x488f,
	0x8b, 0xe5, 0xa0, 0x8a, 0xaf, 0x70, 0xf0, 0x28);
#endif // NTDDI_VERSION < NTDDI_WIN8

//////////////////////////////////////////////////////////////////////////
//
// ESP routines
//
//////////////////////////////////////////////////////////////////////////

static
BOOLEAN
CheckEspListHasKernelGuid(
	PFLT_FILTER		pFilter,
	PECP_LIST		pEcpList,
	LPCGUID			pGuid)
{
	PVOID pEcpContext = nullptr;
	if (!NT_SUCCESS(FltFindExtraCreateParameter(pFilter,
		pEcpList,
		pGuid,
		&pEcpContext,
		nullptr)))
	{
		return FALSE;
	}
	if (FltIsEcpFromUserMode(pFilter, pEcpContext))
	{
		return FALSE;
	}
	return TRUE;
}

BOOLEAN
IsUSBDevice(IN CONST InstanceContext* InstanceContext)
{
	auto nDeviceCharacteristics = InstanceContext->DeviceCharacteristics;
	if ((nDeviceCharacteristics & FILE_REMOVABLE_MEDIA) == 0 &&
		(nDeviceCharacteristics & FILE_PORTABLE_DEVICE) == 0)
	{
		return FALSE;
	}

	return TRUE;
}

static
FLT_PREOP_CALLBACK_STATUS
FLTAPI
SkipPreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
	UNREFERENCED_PARAMETER(CompletionContext);

	if (KeGetCurrentIrql() > PASSIVE_LEVEL ||
		IoGetTopLevelIrp())
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (FLT_IS_FASTIO_OPERATION(Data) ||
		!FLT_IS_IRP_OPERATION(Data))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}


	// Skip	PIPE MAILSLOT VOLUME_OPEN
	if (FlagOn(FltObjects->FileObject->Flags, FO_NAMED_PIPE | FO_MAILSLOT | FO_VOLUME_OPEN))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	// Skip	PAGING_FILE
	if (FlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	// Skip Directory File
	if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE))
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	// Skip	Special cases by ESP
	PECP_LIST ecpList = nullptr;
	if (NT_SUCCESS(FltGetEcpListFromCallbackData(g_pGlobalData->pFileFilter, Data, &ecpList)) &&
		ecpList != nullptr)
	{
		// Skip prefetcher
		if (CheckEspListHasKernelGuid(g_pGlobalData->pFileFilter, ecpList, &GUID_ECP_PREFETCH_OPEN))
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
		// Skip Windows defender csvfs calls
		if (CheckEspListHasKernelGuid(g_pGlobalData->pFileFilter, ecpList, &GUID_ECP_CSV_DOWN_LEVEL_OPEN))
		{
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
	}

	// 如果需要继续，在调用这个函数的时候需要检验返回值是否是FLT_PREOP_SUCCESS_NO_CALLBACK
	// 这里的返回值只是用于默认校验而已
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



BOOLEAN
IsDeleteAllowed(PCUNICODE_STRING Filename)
{
	auto bFind = FileFilter::getInstance()->IsInTable(const_cast<PUNICODE_STRING>(Filename));

	if (bFind)
	{
		return FALSE;
	}
	
	return TRUE;
}

//************************************
// Method:    IsNeedFilter
// FullName:  IsNeedFilter
// Access:    public static 
// Returns:   boolean
// Qualifier:
// Parameter: ACCESS_MASK DesiredAccess
// Parameter: ULONG CreateDisposition
// Parameter: ULONG CreateOptions
// 根据创建文件时的相应参数过滤文件
//************************************
BOOLEAN
IsNeedFilter(
	ACCESS_MASK        DesiredAccess,
	ULONG              CreateDisposition,
	ULONG              CreateOptions)
{
	BOOLEAN bNeedFilter = FALSE;

	// https://learn.microsoft.com/zh-cn/windows/win32/api/winternl/nf-winternl-ntcreatefile
	do
	{
		// CreateDisposition
		// 只允许Open操作保护文件 FILE_OPEN
		if (FILE_OPEN != CreateDisposition)
		{
			bNeedFilter = TRUE;
			break;
		}

		// DesiredAccess
		if ((DesiredAccess & FILE_WRITE_DATA) ||
			(DesiredAccess & FILE_WRITE_ATTRIBUTES) ||
			(DesiredAccess & FILE_WRITE_EA) ||
			(DesiredAccess & FILE_APPEND_DATA) ||
			(DesiredAccess & WRITE_OWNER) ||
			(DesiredAccess & WRITE_DAC) ||
			(DesiredAccess & DELETE))		// 这里其实和下面的重复了
		{
			bNeedFilter = TRUE;
			break;
		}

		// CreateOptions
		if (CreateOptions & FILE_DELETE_ON_CLOSE)
		{
			bNeedFilter = TRUE;
			break;
		}

		break;
	} while (FALSE);

	return bNeedFilter;
}



//************************************
// Method:    DelProtectPreCreate
// FullName:  DelProtectPreCreate
// Access:    public static 
// Returns:   FLT_PREOP_CALLBACK_STATUS
// Qualifier:
// Parameter: PFLT_CALLBACK_DATA Data
// Parameter: PCFLT_RELATED_OBJECTS FltObjects
// Pre Create 中处理验证是否删除保护文件
//************************************
FLT_PREOP_CALLBACK_STATUS
FLTAPI
DelProtectPreCreate(
	PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects)
{
	if (KernelMode == Data->RequestorMode)
	{
		// FltMgr不会在I/O完成期间调用微筛选器驱动程序的操作后回调（如果存在
		// 如果有Post Create 这里也不会调用了
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	BOOLEAN bTrustProcess{ FALSE };
	auto hPid = FltGetRequestorProcessId(Data);
	ProcessContext* processContext = PROCESS_CTX_FIND(ULongToHandle(hPid));
	if (processContext && processContext->ProcessPath.Buffer)
	{
		bTrustProcess = processContext->bTrusted;
	}
	else
	{
		WCHAR wszProcessPath[MAX_PATH]{ 0 };
		auto status = GetProcessImageByPid(ULongToHandle(hPid), wszProcessPath);
		if (!NT_SUCCESS(status))
		{
			bTrustProcess = FALSE;
		}
		else
		{
			bTrustProcess = CRULES_FIND_TRUST_PROCESS(wszProcessPath);
		}
	}


	// FltMgr不会在I/O完成期间调用微筛选器驱动程序的操作后回调（如果存在
	// 如果有Post Create 这里也不会调用了
	auto status = FLT_PREOP_SUCCESS_NO_CALLBACK;

	// https://learn.microsoft.com/zh-cn/windows/win32/api/winternl/nf-winternl-ntcreatefile
	auto params = Data->Iopb->Parameters.Create;


	// 高8位		CreateDisposition
	// 低24位	CreateOptions
	// auto options = params.Options;

	// 新增加对文件覆盖操作的过滤
	// CreateDisposition 这里可以获取到文件的创建/打开方式
	// 也可以用来验证一些操作
	auto const& disposition = params.Options >> 24;

	// ACCESS_MASK        DesiredAccess
	auto const& accessMask = params.SecurityContext->DesiredAccess;

	// CreateOptions
	auto const& createOptions = params.Options & FILE_VALID_OPTION_FLAGS;
	/*
	* 增加一些覆盖/替换文件操作的过滤
	FILE_SUPERSEDE (如果文件已存在，请将其替换为给定的文件。 如果没有，请创建给定的文件)
	FILE_OVERWRITE (如果文件已存在，请将其打开并覆盖。 如果没有，则使请求失败)
	FILE_OVERWRITE_IF (如果文件已存在，请将其打开并覆盖。 如果没有，请创建给定的文件)
	*/
	// FILE_DELETE_ON_CLOSE(将文件的最后一个句柄传递给NtClose时，请删除该文件。如果设置了此标志，则必须在DesiredAccess参数中设置DELETE标志)
	// if (params.SecurityContext->DesiredAccess & DELETE)
	// (options & FILE_VALID_OPTION_FLAGS) 就是为了获取低24位数据 -> CreateOptions
	/*if ((options & FILE_VALID_OPTION_FLAGS) & FILE_DELETE_ON_CLOSE ||
		(disposition == FILE_SUPERSEDE ||
			disposition == FILE_OVERWRITE ||
			disposition == FILE_OVERWRITE_IF))*/
	if (IsNeedFilter(accessMask, disposition, createOptions))
	{
		// 删除操作需要验证文件是否是保护目录/文件，并且是否是允许进程的操作
		auto fileName = &FltObjects->FileObject->FileName;

		// 这里只是过滤了文件全路径的比对
		// 如果需要验证文件是否在保护目录中，需要再增加一个保护目录的表(已增加)
		// 增加了验证是否是受信任进程操作
		if (!IsDeleteAllowed(fileName) &&
			!bTrustProcess)
		{
			// 设置成拒绝访问
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			// 返回FLT_PREOP_COMPLETE时，FltMgr不会将I/O操作发送到驱动程序堆栈中调用方下方的任何微筛选器驱动程序或文件系统
			status = FLT_PREOP_COMPLETE;
		}
	}

	return status;
}

//************************************
// Method:    FileProtectPreSetFileInformation
// FullName:  FileProtectPreSetFileInformation
// Access:    public static 
// Returns:   FLT_PREOP_CALLBACK_STATUS
// Qualifier:
// Parameter: PFLT_CALLBACK_DATA Data
// Parameter: PCFLT_RELATED_OBJECTS FltObjects
// Pre SetFileInformation 中调用处理不允许删除或者重命名保护文件
//************************************
FLT_PREOP_CALLBACK_STATUS
FileProtectPreSetFileInformation(
	PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects)
{
	if (KernelMode == Data->RequestorMode)
	{
		// FltMgr不会在I/O完成期间调用微筛选器驱动程序的操作后回调（如果存在
		// 如果有Post Create 这里也不会调用了
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	BOOLEAN bTrustProcess{ FALSE };
	auto hPid = FltGetRequestorProcessId(Data);
	ProcessContext* processContext = PROCESS_CTX_FIND(ULongToHandle(hPid));
	if (processContext && processContext->ProcessPath.Buffer)
	{
		bTrustProcess = processContext->bTrusted;
	}
	else
	{
		WCHAR wszProcessPath[MAX_PATH]{ 0 };
		auto status = GetProcessImageByPid(ULongToHandle(hPid), wszProcessPath);
		if (!NT_SUCCESS(status))
		{
			bTrustProcess = FALSE;
		}
		else
		{
			bTrustProcess = CRULES_FIND_TRUST_PROCESS(wszProcessPath);
		}
	}

	auto const& params = Data->Iopb->Parameters.SetFileInformation;

	// FltMgr不会在I/O完成期间调用微筛选器驱动程序的操作后回调（如果存在
	// 如果有Post Create 这里也不会调用了
	auto status = FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (params.FileInformationClass == FileDispositionInformation ||
		params.FileInformationClass == 64 /*FileDispositionInformationEx*/)	// 兼容低版本WDK
	{
		auto info = reinterpret_cast<FILE_DISPOSITION_INFORMATION*>(params.InfoBuffer);
		if (info->DeleteFile & TRUE)
		{
			PFLT_FILE_NAME_INFORMATION fi;
			//
			// using FLT_FILE_NAME_NORMALIZED is important here for parsing purposes
			//
			if (NT_SUCCESS(FltGetFileNameInformation(Data,
				FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED,
				&fi)))
			{
				// 示例验证，这块一定需要重写的
				if (!IsDeleteAllowed(&fi->Name) && 
					!bTrustProcess)
				{
					Data->IoStatus.Status = STATUS_ACCESS_DENIED;
					status = FLT_PREOP_COMPLETE;
				}
				FltReleaseFileNameInformation(fi);
			}
		}
	}

	// 一般情况下，保护文件也不允许重命名
	else if (params.FileInformationClass == FileRenameInformation ||
		params.FileInformationClass == 65/*FileRenameInformationEx*/)	// 为了兼容低版本wdk
	{
		// 删除操作需要验证文件是否是保护目录/文件，并且是否是允许进程的操作
		auto fileName = &FltObjects->FileObject->FileName;

		if (!IsDeleteAllowed(fileName) && 
			!bTrustProcess)
		{
			// 设置成拒绝访问
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			// 返回FLT_PREOP_COMPLETE时，FltMgr不会将I/O操作发送到驱动程序堆栈中调用方下方的任何微筛选器驱动程序或文件系统
			status = FLT_PREOP_COMPLETE;
		}
	}


	return status;
}

//************************************
// Method:    FltPreCreate
// FullName:  FltPreCreate
// Access:    public static 
// Returns:   FLT_PREOP_CALLBACK_STATUS
// Qualifier:
// Parameter: _Inout_ PFLT_CALLBACK_DATA Data
// Parameter: _In_ PCFLT_RELATED_OBJECTS FltObjects
// Parameter: _Flt_CompletionContext_Outptr_ PVOID * CompletionContext
//************************************
FLT_PREOP_CALLBACK_STATUS
FLTAPI
FltPreCreate(
	_Inout_	PFLT_CALLBACK_DATA				Data,
	_In_	PCFLT_RELATED_OBJECTS			FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
	UNREFERENCED_PARAMETER(CompletionContext);

	FLT_PREOP_CALLBACK_STATUS resultStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	// 跳过不适合场景
	resultStatus = SkipPreCreate(Data, FltObjects, CompletionContext);
	// check
	if (FLT_PREOP_SUCCESS_NO_CALLBACK == resultStatus)
	{
		*CompletionContext = NULL;
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	// 保护文件 不允许删除, 这里在实际应用中一定还要关联可信任进程操作
	resultStatus = DelProtectPreCreate(Data, FltObjects);
	if (FLT_PREOP_COMPLETE == resultStatus)
	{
		// 看需求是否需要上报
		// ...
		// ...
		// ...


		return FLT_PREOP_COMPLETE;
	}

	// 继续处理其他情况
	*CompletionContext = NULL;
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
FLTAPI
FltPreRead(
	_Inout_	PFLT_CALLBACK_DATA				Data,
	_In_	PCFLT_RELATED_OBJECTS			FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
	UNREFERENCED_PARAMETER(CompletionContext);

	FLT_PREOP_CALLBACK_STATUS FltStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	NTSTATUS status = STATUS_SUCCESS;
	InstanceContext* InstanceContext = nullptr;

	// 判断规则是否允许读移动设备文件
	if (g_pGlobalData->volumeControlFlag & BLOCK_USB_READ)
	{
		status = FltGetInstanceContext(FltObjects->Instance, reinterpret_cast<PFLT_CONTEXT*>(&InstanceContext));
		if (NT_SUCCESS(status))
		{
			// 这里后期需要验证请求的进程是否是白名单允许操作
			if (IsUSBDevice(InstanceContext))
			{
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0;

				FltReleaseContext(InstanceContext);

				// 修改了文件
				FltSetCallbackDataDirty(Data);
				return FLT_PREOP_COMPLETE;
			}

			FltReleaseContext(InstanceContext);
		}

	}



	return FltStatus;
}


FLT_POSTOP_CALLBACK_STATUS
FLTAPI
FltPostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);
	return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
FLTAPI
FltPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
	UNREFERENCED_PARAMETER(CompletionContext);

	FLT_PREOP_CALLBACK_STATUS FltStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	NTSTATUS status = STATUS_SUCCESS;
	InstanceContext* InstanceContext = nullptr;

	// 开启了写保护
	if (g_pGlobalData->volumeControlFlag & BLOCK_USB_WRITE)
	{
		status = FltGetInstanceContext(FltObjects->Instance, reinterpret_cast<PFLT_CONTEXT*>(&InstanceContext));
		if (NT_SUCCESS(status))
		{
			// 这里后期需要添加验证是否是白名单进程进行操作
			if (IsUSBDevice(InstanceContext))
			{
				FltReleaseContext(InstanceContext);
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0;
				FltSetCallbackDataDirty(Data);
				return FLT_PREOP_COMPLETE;
			}

			FltReleaseContext(InstanceContext);
		}
	}


	return FltStatus;
}

//************************************
// Method:    FltPreSetFileInformation
// FullName:  FltPreSetFileInformation
// Access:    public static 
// Returns:   FLT_PREOP_CALLBACK_STATUS
// Qualifier:
// Parameter: _Inout_ PFLT_CALLBACK_DATA Data
// Parameter: _In_ PCFLT_RELATED_OBJECTS FltObjects
// Parameter: _Flt_CompletionContext_Outptr_ PVOID * CompletionContext
//************************************
FLT_PREOP_CALLBACK_STATUS
FLTAPI
FltPreSetFileInformation(
	_Inout_	PFLT_CALLBACK_DATA				Data,
	_In_	PCFLT_RELATED_OBJECTS			FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
	UNREFERENCED_PARAMETER(CompletionContext);

	// 由于我们并没有Post操作，所以默认返回值设置为FLT_PREOP_SUCCESS_NO_CALLBACK
	FLT_PREOP_CALLBACK_STATUS resultStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	resultStatus = FileProtectPreSetFileInformation(Data, FltObjects);
	if (FLT_PREOP_SUCCESS_NO_CALLBACK != resultStatus)
	{

		// log 
		return resultStatus;
	}


	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

NTSTATUS
FLTAPI
InstanceSetupCallback(
	PCFLT_RELATED_OBJECTS FltObjects,
	FLT_INSTANCE_SETUP_FLAGS Flags,
	DEVICE_TYPE VolumeDeviceType,
	FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);

	NTSTATUS status = STATUS_SUCCESS;

	if (VolumeDeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM ||
		VolumeDeviceType == FILE_DEVICE_NETWORK_REDIRECTOR ||
		VolumeDeviceType == FILE_DEVICE_DFS_FILE_SYSTEM)
	{
		return STATUS_FLT_DO_NOT_ATTACH;
	}

	if (VolumeFilesystemType != FLT_FSTYPE_NTFS &&
		VolumeFilesystemType != FLT_FSTYPE_FAT)
	{
		return STATUS_FLT_DO_NOT_ATTACH;
	}

	InstanceContext* pInstanceContext = nullptr;

	auto volume = FltObjects->Volume;
	FLT_VOLUME_PROPERTIES* pVolumeProperties = nullptr;
	ULONG returnLength = 0;
	const ULONG volumePropertiesTag = 'mpmV';
	do
	{
		// 申请InstanceCtx
		status = FltAllocateContext(g_pGlobalData->pFileFilter,
			FLT_INSTANCE_CONTEXT,
			INSTANCE_CONTEXT_SIZE,
			NonPagedPoolNx,
			reinterpret_cast<PFLT_CONTEXT*>(&pInstanceContext));
		if (NT_SUCCESS(status))
		{
			
			// STATUS_BUFFER_OVERFLOW
			// volumeProperties 选择指针而非结构体否者在判断NT_SUCCESS的时候：STATUS_BUFFER_OVERFLOW
			// https://learn.microsoft.com/zh-cn/windows-hardware/drivers/ddi/fltkernel/nf-fltkernel-fltgetvolumeproperties
			status = FltGetVolumeProperties(volume, NULL, 0, &returnLength);
			if (STATUS_BUFFER_TOO_SMALL == status)
			{
				pVolumeProperties = reinterpret_cast<FLT_VOLUME_PROPERTIES*>(ExAllocatePoolWithTag(NonPagedPoolNx, returnLength, volumePropertiesTag));
				if (!pVolumeProperties)
				{
					return STATUS_INSUFFICIENT_RESOURCES;
				}
				RtlZeroMemory(pVolumeProperties, returnLength);

				status = FltGetVolumeProperties(volume, pVolumeProperties, returnLength, &returnLength);
				if (!NT_SUCCESS(status))
				{
					break;
				}
			}

			pInstanceContext->Tag = INSTANCE_CONTEXT_TAG;
			// 比较重要的点
			pInstanceContext->DeviceCharacteristics = pVolumeProperties->DeviceCharacteristics;
			// 比较重要的点
			if (0 == pVolumeProperties->SectorSize)
			{
				pInstanceContext->SectorSize = PAGE_SIZE;
			}
			pInstanceContext->VolumeFilesystemType = VolumeFilesystemType;

			// 设置
			status = FltSetInstanceContext(FltObjects->Instance, FLT_SET_CONTEXT_KEEP_IF_EXISTS, pInstanceContext, nullptr);
			
			// Always release the context, regardless of FltSetInstanceContext.
			// If FltSetInstanceContext succeeds, it takes a reference, and if
			// it fails, FltReleaseContext will delete the context.
			FltReleaseContext(pInstanceContext);
		}

	} while (FALSE);

	if (pVolumeProperties)
	{
		ExFreePoolWithTag(pVolumeProperties, volumePropertiesTag);
		pVolumeProperties = nullptr;
	}


	return status;
}


VOID
ContextCleanup(
	PFLT_CONTEXT Context,
	FLT_CONTEXT_TYPE ContextType)
{
	UNREFERENCED_PARAMETER(Context);
	UNREFERENCED_PARAMETER(ContextType);
}




CONST FLT_OPERATION_REGISTRATION c_Callbacks[] =
{
	{ IRP_MJ_CREATE,
	  0,
	  FltPreCreate,
	  nullptr
	},

	/*{ IRP_MJ_READ,
	  0,
	  FltPreRead,
	  FltPostRead
	},

	{ IRP_MJ_WRITE,
	  0,
	  FltPreWrite,
	  nullptr
	},*/

	//{ IRP_MJ_CLEANUP, 0, preCleanup, postCleanup },
	{ IRP_MJ_SET_INFORMATION,
	  FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
	  FltPreSetFileInformation,
	  nullptr
	},

	//{ IRP_MJ_WRITE, 0, preWrite, postWrite },
	//{ IRP_MJ_READ, 0, preRead, postRead },

	{ IRP_MJ_OPERATION_END }
};

const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {
	{ FLT_INSTANCE_CONTEXT,
	  0,
	  ContextCleanup,
	  INSTANCE_CONTEXT_SIZE,
	  INSTANCE_CONTEXT_TAG
	},

	{FLT_CONTEXT_END}
};


//
//  This defines what we want to filter with FltMgr
//
CONST FLT_REGISTRATION C_FilterRegistration =
{
	sizeof(FLT_REGISTRATION),			//  Size
	FLT_REGISTRATION_VERSION,			//  Version
	0,									//  eFlags

	ContextRegistration,				//  Context
	c_Callbacks,						//  Operation callbacks

	UnloadFilter,						//  MiniFilterUnload

	InstanceSetupCallback,				//  InstanceSetup
	nullptr,							//  InstanceQueryTeardown
	nullptr,							//  InstanceTeardownStart
	nullptr,							//  InstanceTeardownComplete

	NULL,								//  GenerateFileName
	NULL,								//  GenerateDestinationFileName
	NULL								//  NormalizeNameComponent
};


NTSTATUS 
FileFilter::IntializedFileFilter()
{
	NTSTATUS status{ STATUS_SUCCESS };
	
	m_tableOfProtectFile = new(NonPagedPoolNx) GenericTable<ProtectFile, _CompareProtectFile>;
	if (!m_tableOfProtectFile)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = FltRegisterFilter(g_pGlobalData->pDriverObject, 
		&C_FilterRegistration,
		&g_pGlobalData->pFileFilter);
	if (!NT_SUCCESS(status))
	{
		LOGERROR(status, "FltRegisterFilter failed\r\n");
		return status;
	}

	status = FltStartFiltering(g_pGlobalData->pFileFilter);
	if (!NT_SUCCESS(status))
	{
		LOGERROR(status, "FltStartFiltering failed\r\n");
	}

	return status;
}

NTSTATUS 
FileFilter::FinalizedFileFilter()
{
	if (m_tableOfProtectFile)
	{
		delete m_tableOfProtectFile;
	}

	if (g_pGlobalData->pFileFilter)
	{
		FltUnregisterFilter(g_pGlobalData->pFileFilter);
		g_pGlobalData->pFileFilter = nullptr;
	}

	return STATUS_SUCCESS;
}

