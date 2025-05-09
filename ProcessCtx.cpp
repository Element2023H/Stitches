#include "ProcessCtx.hpp"
#include "Imports.hpp"
#include "Utils.hpp"
#include "Log.hpp"

extern GlobalData* g_pGlobalData;

VOID ProcessCtx::Initialization()
{
	ExInitializeNPagedLookasideList(&g_pGlobalData->ProcessCtxNPList,
		nullptr,
		nullptr,
		0,
		ProcessContextSize,
		ProcessContextTag,
		0);

	InitializeListHead(&g_pGlobalData->ProcessCtxList);
	ExInitializeFastMutex(&g_pGlobalData->ProcessCtxFastMutex);
}



VOID
ProcessCtx::AddProcessContext(
	IN CONST			PEPROCESS	Process,
	IN CONST			HANDLE		Pid,
	IN OUT OPTIONAL		PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	ProcessContext* pProcessCtx = reinterpret_cast<ProcessContext*>(ExAllocateFromNPagedLookasideList(&g_pGlobalData->ProcessCtxNPList));
	if (!pProcessCtx)
	{
		LOGERROR(STATUS_INSUFFICIENT_RESOURCES, "ExAllocateFromNPagedLookasideList");
		return;
	}
	RtlZeroMemory(pProcessCtx, ProcessContextSize);

	pProcessCtx->Pid = Pid;
	pProcessCtx->bProtected = IsProtectedProcess(Process);

	if (g_pGlobalData->PsGetProcessWow64Process)
	{
		pProcessCtx->bIsWow64 = (g_pGlobalData->PsGetProcessWow64Process(Process) != nullptr);
	}

	do
	{
		// process image file name
		pProcessCtx->ProcessPath.Buffer = reinterpret_cast<PWCH>(ExAllocatePoolWithTag(PagedPool, CreateInfo->ImageFileName->MaximumLength + sizeof(UNICODE_STRING), ProcessContextTag));
		if (pProcessCtx->ProcessPath.Buffer)
		{
			RtlZeroMemory(pProcessCtx->ProcessPath.Buffer, CreateInfo->ImageFileName->MaximumLength + sizeof(UNICODE_STRING));
			pProcessCtx->ProcessPath.Length = 0;
			pProcessCtx->ProcessPath.MaximumLength = CreateInfo->ImageFileName->MaximumLength;
			RtlCopyUnicodeString(&pProcessCtx->ProcessPath, CreateInfo->ImageFileName);
		}
		else
		{
			LOGERROR(STATUS_INSUFFICIENT_RESOURCES, "[ProcessCtx ERROR]ProcessCtx ProcessPath buffer alloc failed\r\n");
			break;
		}


		// process commandline
		pProcessCtx->ProcessCmdLine.Buffer = reinterpret_cast<PWCH>(ExAllocatePoolWithTag(PagedPool, CreateInfo->CommandLine->MaximumLength + sizeof(UNICODE_STRING), ProcessContextTag));
		if (pProcessCtx->ProcessCmdLine.Buffer)
		{
			RtlZeroMemory(pProcessCtx->ProcessCmdLine.Buffer, CreateInfo->CommandLine->MaximumLength + sizeof(UNICODE_STRING));
			pProcessCtx->ProcessCmdLine.Length = 0;
			pProcessCtx->ProcessCmdLine.MaximumLength = CreateInfo->CommandLine->MaximumLength;
			RtlCopyUnicodeString(&pProcessCtx->ProcessCmdLine, CreateInfo->CommandLine);
		}
		else
		{
			LOGERROR(STATUS_INSUFFICIENT_RESOURCES, "[ProcessCtx ERROR]ProcessCtx cmdline buffer alloc failed\r\n");
			break;
		}


		ExAcquireFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

		InsertHeadList(&g_pGlobalData->ProcessCtxList, &pProcessCtx->ListHeader);

		ExReleaseFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

		return;
	} while (FALSE);


	// failed 
	if (pProcessCtx->ProcessPath.Buffer)
	{
		ExFreePoolWithTag(pProcessCtx->ProcessPath.Buffer, ProcessContextTag);
		pProcessCtx->ProcessPath.Buffer = nullptr;
	}


	ExFreeToNPagedLookasideList(&g_pGlobalData->ProcessCtxNPList, pProcessCtx);

}


VOID
ProcessCtx::DeleteProcessCtxByPid(IN CONST HANDLE ProcessId)
{
	if (!ProcessId || IsListEmpty(&g_pGlobalData->ProcessCtxList))
	{
		return;
	}

	PLIST_ENTRY pEntry = g_pGlobalData->ProcessCtxList.Flink;

	while (pEntry != &g_pGlobalData->ProcessCtxList)
	{
		ProcessContext* pNode = CONTAINING_RECORD(pEntry, ProcessContext, ListHeader);
		if (pNode)
		{
			if (ProcessId == pNode->Pid)
			{
				if (pNode->ProcessPath.Buffer)
				{
					ExFreePoolWithTag(pNode->ProcessPath.Buffer, ProcessContextTag);
					pNode->ProcessPath.Buffer = nullptr;
				}

				if (pNode->ProcessCmdLine.Buffer)
				{
					ExFreePoolWithTag(pNode->ProcessCmdLine.Buffer, ProcessContextTag);
					pNode->ProcessCmdLine.Buffer = nullptr;
				}

				ExAcquireFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

				RemoveEntryList(&pNode->ListHeader);

				ExReleaseFastMutex(&g_pGlobalData->ProcessCtxFastMutex);


				ExFreeToNPagedLookasideList(&g_pGlobalData->ProcessCtxNPList, pNode);

				break;
			}
		}
		if (pEntry)
		{
			pEntry = pEntry->Flink;
		}
	}
}

ProcessContext*
ProcessCtx::FindProcessCtxByPid(IN CONST HANDLE Pid)
{
	if (!Pid || IsListEmpty(&g_pGlobalData->ProcessCtxList))
	{
		return nullptr;
	}

	ProcessContext* pNode{ nullptr };
	PLIST_ENTRY			pEntry = g_pGlobalData->ProcessCtxList.Flink;

	ExAcquireFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

	while (pEntry != &g_pGlobalData->ProcessCtxList)
	{
		pNode = CONTAINING_RECORD(pEntry, ProcessContext, ListHeader);
		if (pNode && (Pid == pNode->Pid))
		{
			ExReleaseFastMutex(&g_pGlobalData->ProcessCtxFastMutex);
			return pNode;
		}

		pEntry = pEntry->Flink;
	}

	ExReleaseFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

	return pNode;
}


VOID
ProcessCtx::CleanupProcessCtxList()
{
	while (!IsListEmpty(&g_pGlobalData->ProcessCtxList))
	{
		PLIST_ENTRY pEntry = g_pGlobalData->ProcessCtxList.Flink;
		ProcessContext* pNode = CONTAINING_RECORD(pEntry, ProcessContext, ListHeader);
		if (pNode)
		{

			if (pNode->ProcessPath.Buffer)
			{
				ExFreePoolWithTag(pNode->ProcessPath.Buffer, ProcessContextTag);
				pNode->ProcessPath.Buffer = nullptr;
			}

			if (pNode->ProcessCmdLine.Buffer)
			{
				ExFreePoolWithTag(pNode->ProcessCmdLine.Buffer, ProcessContextTag);
				pNode->ProcessCmdLine.Buffer = nullptr;
			}

			ExAcquireFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

			RemoveEntryList(&pNode->ListHeader);

			ExReleaseFastMutex(&g_pGlobalData->ProcessCtxFastMutex);

			ExFreeToNPagedLookasideList(&g_pGlobalData->ProcessCtxNPList, pNode);
		}
	}
}