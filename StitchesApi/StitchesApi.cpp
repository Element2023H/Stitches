// StitchesApi.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "framework.h"
#include "StitchesApi.h"
#include "Common.h"
#include "Sync.hpp"


using namespace StitchesApi;

static  AutoHandle	g_hDevice;

// 这是已导出类的构造函数。
CStitchesApi::CStitchesApi()
{
    return;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::AddTrustProcess(CONST std::wstring& ProcessPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_ADD_TRUST_PROCESS,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
		ProcessPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::DelTrustProcess(CONST std::wstring& ProcessPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_DEL_TRUST_PROCESS,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
		ProcessPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::AddProtectProcess(CONST std::wstring& ProcessPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_ADD_PROTECT_PROCESS,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
		ProcessPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::DelProtectProcess(CONST std::wstring& ProcessPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_DEL_PROTECT_PROCESS,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
		ProcessPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::AddProtectFile(CONST std::wstring& ProcessPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_ADD_PROTECT_FILE,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
		ProcessPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::DelProtectFile(CONST std::wstring& ProcessPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_DEL_PROTECT_FILE,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
		ProcessPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::AddProtectRegistry(CONST std::wstring& RegistryPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_ADD_PROTECT_REGISTRY,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(RegistryPath.c_str())),
		RegistryPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN 
STITCHESAPI_CC 
CStitchesApi::DelProtectRegistry(CONST std::wstring& RegistryPath)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_DEL_PROTECT_REGISTRY,
		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(RegistryPath.c_str())),
		RegistryPath.length() * sizeof(WCHAR),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

BOOLEAN
STITCHESAPI_CC
CStitchesApi::SetHookDllPath(
	CONST std::wstring& x64dll,
	CONST std::wstring& x86dll)
{
	DWORD	dwBytesReturned{ 0 };
	if (INVALID_HANDLE_VALUE == g_hDevice)
	{
		return FALSE;
	}

	HOOK_DLL_PATH hookDllPath{};

	memcpy(hookDllPath.x64Dll, x64dll.c_str(), x64dll.length() * sizeof(WCHAR));
	memcpy(hookDllPath.x86Dll, x86dll.c_str(), x86dll.length() * sizeof(WCHAR));

	if (!DeviceIoControl(g_hDevice,
		IOCTL_STITCHES_DEL_PROTECT_FILE,
		reinterpret_cast<LPVOID>(&hookDllPath),
		sizeof(HOOK_DLL_PATH),
		nullptr,
		0,
		&dwBytesReturned,
		nullptr))
	{
		return FALSE;
	}


	return TRUE;
}

//STITCHESAPI_API
//BOOLEAN
//STITCHESAPI_NS
//AddTrustProcess(CONST std::wstring& ProcessPath)
//{
//	DWORD	dwBytesReturned{ 0 };
//	if (INVALID_HANDLE_VALUE == g_hDevice)
//	{
//		return FALSE;
//	}
//
//	if (!DeviceIoControl(g_hDevice,
//		IOCTL_STITCHES_ADD_TRUST_PROCESS,
//		reinterpret_cast<LPVOID>(const_cast<PWCHAR>(ProcessPath.c_str())),
//		ProcessPath.length() * sizeof(WCHAR),
//		nullptr,
//		0,
//		&dwBytesReturned,
//		nullptr))
//	{
//		return FALSE;
//	}
//
//
//	return TRUE;
//}