#pragma once
#include "Lazy.hpp"
#include "Imports.hpp"
#include "Common.h"
#include "EventQueue.hpp"
#include "SystemWorker.hpp"

// 参考 openedr

struct FltPortData
{
	// CommunicationPort
	PFLT_PORT pServerPort{ nullptr };			//< Filter server port 
	PFLT_PORT pClientPort{ nullptr };			//< Filter client port 
	PEPROCESS pClientProcess{ nullptr };
	ULONG	  nFltPortSendMessageTimeout = 2000;

	EventQueue		eventQueue{};				// 消息队列
	SystemWorker	workerThread{};				// 工作线程
	ULONG64			nSleepEndTime{ 0 };			//< Send messages is denied before this time (getTickCount) 
	ULONG64			nMonitoringEndTime{ 0 };	//< Monitoring will be stopped after this time (getTickCount)  
	ULONG64			nNextStatisticLogTime{ 0 };	//< Next time to log statistic
};

static LazyInstance<FltPortData> g_FltPortData;


class Alpc
{
public:
	Alpc() = default;
	~Alpc();

	NTSTATUS Init();

public:
	NTSTATUS
	SendMessageToClient(
		IN EventData& SendMessage,
		IN OUT	 PVOID	 ReplyData /*= nullptr*/,
		IN CONST BOOLEAN IsSync /*= FALSE*/);


protected:
	static
	NTSTATUS
	NotifyOnConnect(
		IN PFLT_PORT ClientPort,
		IN PVOID ServerPortCookie,
		IN PVOID ConnectionContext,
		IN ULONG SizeOfContext,
		OUT PVOID* ConnectionPortCookie);

	static
	VOID
	NotifyOnDisconnect(
		IN PVOID ConnectionCookie);

	static
	NTSTATUS
	NotifyOnMessage(
		IN PVOID PortCookie,
		IN PVOID InputBuffer OPTIONAL,
		IN ULONG InputBufferLength,
		OUT PVOID OutputBuffer OPTIONAL,
		IN ULONG OutputBufferLength,
		OUT PULONG ReturnOutputBufferLength);

	static
	DoWorkResult SendNextEvent(PVOID Context);

	static
	NTSTATUS
	SendMessageToClientSyncNoReply(IN EventData& SendMessage);

	static
	NTSTATUS
	SendMessageToClientSyncNeedReply(
		IN EventData& SendMessage,
		IN OUT PVOID ReplyData);
};
static LazyInstance<Alpc> alpc;

