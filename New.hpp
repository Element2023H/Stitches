#pragma once

#include <ntifs.h>

constexpr ULONG GLOBALDATA_TAG = 'tdGS';

// https://github.com/virtio-win/kvm-guest-drivers-windows/blob/master/viogpu/common/baseobj.cpp

_When_((PoolType& NonPagedPoolMustSucceed) != 0,
	__drv_reportError("Must succeed pool allocations are forbidden. "
		"Allocation failures cause a system crash"))
	auto __cdecl operator new(size_t Size, POOL_TYPE PoolType)->PVOID
{
	Size = (Size != 0) ? Size : 1;

	return ExAllocatePoolZero(PoolType, Size, GLOBALDATA_TAG);
}

_When_((PoolType& NonPagedPoolMustSucceed) != 0,
	__drv_reportError("Must succeed pool allocations are forbidden. "
		"Allocation failures cause a system crash"))
	auto __cdecl operator new[](size_t Size, POOL_TYPE PoolType)->PVOID
{

	Size = (Size != 0) ? Size : 1;

	return ExAllocatePoolZero(PoolType, Size, GLOBALDATA_TAG);
}

void __cdecl operator delete(void* pObject)
{

	if (pObject)
	{
		ExFreePoolWithTag(pObject, GLOBALDATA_TAG);
	}
}

void __cdecl operator delete[](void* pObject)
{

	if (pObject)
	{
		ExFreePoolWithTag(pObject, GLOBALDATA_TAG);
	}
}

void __cdecl operator delete(void* pObject, size_t Size)
{

	UNREFERENCED_PARAMETER(Size);
	::operator delete (pObject);
}