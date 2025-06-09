#include "Alpc.hpp"
#include "Common.h"

extern LazyInstance<GlobalData> g_pGlobalData;

Alpc::~Alpc()
{
	if (g_PortData->pServerPort)
	{
		FltCloseCommunicationPort(g_PortData->pServerPort);
		g_PortData->pServerPort = nullptr;
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
										&g_PortData->pServerPort,
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
	return status;
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
		g_PortData->pClientPort = ClientPort;
	}
	g_PortData->pClientProcess = PsGetCurrentProcess();

	return status;
}

VOID 
Alpc::NotifyOnDisconnect(IN PVOID ConnectionCookie)
{
	UNREFERENCED_PARAMETER(ConnectionCookie);
	if (g_PortData->pClientPort)
	{
		FltCloseClientPort(g_pGlobalData->pFileFilter, &g_PortData->pClientPort);
	}
	g_PortData->pClientProcess = nullptr;
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
