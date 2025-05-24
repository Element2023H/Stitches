#pragma once
#include "Imports.hpp"

WCHAR*
KWstrnstr(
	const WCHAR* src,
	const WCHAR* find);

PVOID 
KGetProcAddress(
	IN CONST HANDLE ModuleHandle, 
	CONST PCHAR FuncName);


NTSTATUS 
KGetDosProcessPath(
	IN PWCHAR DeviceFileName,
	OUT PWCHAR DosFileName);


NTSTATUS
GetProcessImageByPid(
	IN CONST HANDLE Pid, 
	IN OUT PWCHAR ProcessImage);


NTSTATUS 
GetProcessImage(
	IN CONST PEPROCESS Process, 
	IN OUT PWCHAR ProcessImage);


BOOLEAN
UnicodeStringContains(
	PUNICODE_STRING UnicodeString,
	PCWSTR SearchString);



BOOLEAN
IsProtectedProcess(IN CONST PEPROCESS Process);

_IRQL_requires_same_ 
_IRQL_requires_(PASSIVE_LEVEL) 
NTSTATUS 
KTerminateProcess(IN CONST ULONG ProcessId);



BOOLEAN
KGetRegistryPath(
	IN			PVOID	RegistryObject, 
	IN OUT		PWCHAR	Buffer, 
	IN CONST	ULONG	BufferSize );