#pragma once
#include "Lazy.hpp"
#include "Imports.hpp"



struct FltPortData
{
	// CommunicationPort
	PFLT_PORT pServerPort{ nullptr };			//< Filter server port 
	PFLT_PORT pClientPort{ nullptr };			//< Filter client port 
	PEPROCESS pClientProcess{ nullptr };
};

static LazyInstance<FltPortData> g_PortData;


class Alpc
{
public:
	Alpc() = default;
	~Alpc();

	NTSTATUS Init();

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

};
static LazyInstance<Alpc> alpc;

