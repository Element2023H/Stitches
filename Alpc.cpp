#include "Alpc.hpp"
#include "Common.h"

extern LazyInstance<GlobalData> g_pGlobalData;

Alpc::~Alpc()
{
	if (g_FltPortData->pServerPort)
	{
		FltCloseCommunicationPort(g_FltPortData->pServerPort);
		g_FltPortData->pServerPort = nullptr;
	}
}

NTSTATUS
Alpc::Init()
{
	NTSTATUS status{ STATUS_SUCCESS };

	PSECURITY_DESCRIPTOR	pSd{ nullptr };
	OBJECT_ATTRIBUTES		oa{};
	status = FltBuildDefaultSecurityDescriptor(&pSd, FLT_PORT_ALL_ACCESS);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	UNICODE_STRING ustrFltPortName{};
	RtlInitUnicodeString(&ustrFltPortName, DRIVER_FLT_PORT_NAME);

	InitializeObjectAttributes(&oa, &ustrFltPortName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, pSd);

	status = FltCreateCommunicationPort(g_pGlobalData->pFileFilter,
										&g_FltPortData->pServerPort,
										&oa,
										nullptr,
										NotifyOnConnect,
										NotifyOnDisconnect,
										NotifyOnMessage,
										1);
	if (pSd)
	{
		FltFreeSecurityDescriptor(pSd);
	}

	g_FltPortData->eventQueue.Initialize();
	g_FltPortData->workerThread.Initialize(&SendNextEvent, nullptr, 1000);


	return status;
}


NTSTATUS 
Alpc::SendMessageToClient(
	IN EventData& SendMessage, 
	IN OUT PVOID ReplyData /*= nullptr*/, 
	IN CONST BOOLEAN IsSync /*= FALSE*/)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;


	// 同步的方式
	if (IsSync)
	{
		if (ReplyData)
		{
			status = SendMessageToClientSyncNeedReply(SendMessage, ReplyData);
		}
		else
		{
			status = SendMessageToClientSyncNoReply(SendMessage);
		}

	}
	else
	{
		// 异步的方式（需要消息队列支持）
		status = g_FltPortData->eventQueue.Pushback(SendMessage);
		if (NT_SUCCESS(status) && g_FltPortData->pClientPort)
		{
			g_FltPortData->workerThread.Notify();
		}
	}


	return status;
}

NTSTATUS
Alpc::SendMessageToClientSyncNoReply(IN EventData& SendMessage)
{
	NTSTATUS sendResult = STATUS_UNSUCCESSFUL;


	// 设置发送时间
	LARGE_INTEGER liTimeout = {};
	liTimeout.QuadPart = g_FltPortData->nFltPortSendMessageTimeout *
		(LONGLONG)1000 /*micro*/ * (LONGLONG)10 /*100 nano*/ * (LONGLONG)-1/*relative*/;


	// 以不需要回复的方式进行发送
	sendResult = FltSendMessage(g_pGlobalData->pFileFilter,
		&g_FltPortData->pClientPort,
		SendMessage.pDataBuffer,
		SendMessage.uDataLength,
		nullptr, nullptr, &liTimeout);

	return sendResult;
}

NTSTATUS
Alpc::SendMessageToClientSyncNeedReply(
	IN EventData& SendMessage,
	IN OUT PVOID ReplyData)
{
	NTSTATUS sendResult = STATUS_UNSUCCESSFUL;


	// 设置发送时间
	LARGE_INTEGER liTimeout = {};
	liTimeout.QuadPart = (LONGLONG)g_FltPortData->nFltPortSendMessageTimeout *
		(LONGLONG)1000 /*micro*/ * (LONGLONG)10 /*100 nano*/ * (LONGLONG)-1/*relative*/;

	// 回复数据长度
	ULONG uReplyLength = 0;

	// 以回复的方式进行发送
	sendResult = FltSendMessage(g_pGlobalData->pFileFilter,
		&g_FltPortData->pClientPort,
		SendMessage.pDataBuffer,
		SendMessage.uDataLength,
		ReplyData,
		&uReplyLength,
		&liTimeout);

	return sendResult;

}



DoWorkResult Alpc::SendNextEvent(PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	if (!g_FltPortData->pClientPort)
	{
		if (g_FltPortData->nMonitoringEndTime == 0)
		{
			return DoWorkResult::NoWork;
		}

		const ULONG64 nTime = KeQueryInterruptTime();
		auto nTickCount = nTime / ((ULONG64)10 /*100 nano*/ * (ULONG64)1000 /*micro*/);

		if (nTickCount > g_FltPortData->nMonitoringEndTime)
		{
			// 关闭监控
		}
		return DoWorkResult::NoWork;
	}


	// No event - returns no work
	EventData rawEvent;
	if (!g_FltPortData->eventQueue.GetFront(rawEvent))
		return DoWorkResult::NoWork;

	// Sleep time - returns no work
	if (g_FltPortData->nSleepEndTime != 0)
	{
		const ULONG64 nTime = KeQueryInterruptTime();
		auto nTickCount = nTime / ((ULONG64)10 /*100 nano*/ * (ULONG64)1000 /*micro*/);
		if (nTickCount < g_FltPortData->nSleepEndTime)
			return DoWorkResult::NoWork;
		g_FltPortData->nSleepEndTime = 0; // reset time
	}

	// 异步的方式是没有回复数据的
	const NTSTATUS eSendResult = SendMessageToClientSyncNoReply(rawEvent);

	// Timeout is occurred - send NextEvent again immediately (timeout was already waited).
	if (eSendResult == STATUS_TIMEOUT)
	{
		// LOGINFO2("[WARN] FltSendMessage timeout is occured.\r\n");
		return DoWorkResult::HasWork;
	}

	// Disconnection - stops sending.
	if (eSendResult == STATUS_PORT_DISCONNECTED)
	{
		// LOGINFO2("[WARN] FltSendMessage disconnection is occured.\r\n");
		return DoWorkResult::NoWork;
	}

	// Unexpected error - starts sleep time.
	if (!NT_SUCCESS(eSendResult))
	{
		const ULONG64 nTime = KeQueryInterruptTime();
		auto nTickCount = nTime / ((ULONG64)10 /*100 nano*/ * (ULONG64)1000 /*micro*/);
		g_FltPortData->nSleepEndTime = nTickCount + g_FltPortData->nFltPortSendMessageTimeout;
		//LOGERROR(eSendResult, "FltSendMessage returns error.\r\n");
		return DoWorkResult::NoWork;
	}

	// Success - removes event and returns actual status.
	g_FltPortData->eventQueue.PopFront();
	return g_FltPortData->eventQueue.IsEmpty() ? DoWorkResult::NoWork : DoWorkResult::HasWork;
}

NTSTATUS 
Alpc::NotifyOnConnect(
	IN PFLT_PORT ClientPort, 
	IN PVOID ServerPortCookie, 
	IN PVOID ConnectionContext, 
	IN ULONG SizeOfContext, 
	OUT PVOID* ConnectionPortCookie)
{
	UNREFERENCED_PARAMETER(ClientPort);
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionPortCookie);

	NTSTATUS status{ STATUS_SUCCESS };

	if (ConnectionPortCookie)
	{
		*ConnectionPortCookie = nullptr;
	}

	if (ClientPort)
	{
		g_FltPortData->pClientPort = ClientPort;
	}
	g_FltPortData->pClientProcess = PsGetCurrentProcess();

	return status;
}

VOID 
Alpc::NotifyOnDisconnect(IN PVOID ConnectionCookie)
{
	UNREFERENCED_PARAMETER(ConnectionCookie);
	if (g_FltPortData->pClientPort)
	{
		FltCloseClientPort(g_pGlobalData->pFileFilter, &g_FltPortData->pClientPort);
	}
	g_FltPortData->pClientProcess = nullptr;
}

NTSTATUS 
Alpc::NotifyOnMessage(
	IN PVOID PortCookie,
	IN PVOID InputBuffer OPTIONAL, 
	IN ULONG InputBufferLength, 
	OUT PVOID OutputBuffer OPTIONAL, 
	IN ULONG OutputBufferLength,
	OUT PULONG ReturnOutputBufferLength)
{
	UNREFERENCED_PARAMETER(PortCookie);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);
	
	// handle message from client

	return STATUS_SUCCESS;
}
