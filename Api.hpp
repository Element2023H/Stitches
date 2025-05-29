#pragma once
#include <ntifs.h>
#include "Traits.hpp"

using namespace traits;

template <class _Fty, class _GetFunctionTrait = GetFunction>
class NtFunction;

template <class _Fty, class _GetFunctionTrait>
class NtFunction
{
	static_assert(_Always_false<_Fty>, "invalid generic parameter _Fty");
};

/// <summary>
/// A type that define the default method to get system routine address
/// </summary>
struct GetFunction
{
	PVOID operator()(const WCHAR* Name)
	{
		UNICODE_STRING FuncName;

		RtlInitUnicodeString(&FuncName, Name);

		auto FuncPtr = MmGetSystemRoutineAddress(&FuncName);

		return FuncPtr;
	}
};

/// <summary>
/// A type that define the default method to get system routine address with IRQL check
/// it will BugCheck when IRQL is not equal to PASSIVE_LEVEL since `MmGetSystemRoutineAddress` can only run on this level
/// </summary>
struct GetFunctionChecked
{
	PVOID operator()(const WCHAR* Name)
	{
		NT_ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

		if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		{
			KeBugCheck(IRQL_NOT_LESS_OR_EQUAL);
		}

		return GetFunction{}(Name);
	}
};

/// <summary>
/// A type that define method to get system routine address from parsing export table from ntoskrnl.exe
/// this method is not implemented, u should implement it yourself
/// </summary>
struct GetFunctionPEExport
{
	static constexpr ULONG BUGCHECK_NOT_IMPLEMENTED = 0x800;

	PVOID operator()(const WCHAR* Name)
	{
		UNREFERENCED_PARAMETER(Name);

		KeBugCheck(BUGCHECK_NOT_IMPLEMENTED);
	}
};

/// <summary>
/// zero-cost wrapper for calling exported function from ntoskrnl.exe
/// it will bugcheck if failed to get the system routine address
/// </summary>
/// <typeparam name="R">return value type of the target function</typeparam>
/// <typeparam name="...Args">arguments type of the target function</typeparam>
template <class _GetFunction, class R, class... Args>
class NtFunction<R(*)(Args...), _GetFunction>
{
	using function_type = R(NTAPI*)(Args...);
public:
	FORCEINLINE
	static NtFunction Force(const WCHAR* Name)
	{
		return NtFunction{ reinterpret_cast<function_type>(_GetFunction{}(Name)) };
	}

	FORCEINLINE
	void Init(const WCHAR* Name)
	{
		this->m_function = reinterpret_cast<function_type>(_GetFunction{}(Name));
	}

	FORCEINLINE
	bool Empty() const
	{
		return m_function == nullptr;
	}

	FORCEINLINE
	operator bool() const
	{
		return !this->Empty();
	}

	FORCEINLINE
	R operator () (Args... args) const
	{
		return this->call(args...);
	}

	FORCEINLINE
	R operator () (Args... args)
	{
		return this->call(args...);
	}

	FORCEINLINE
	R call(Args... args) const
	{
		return (*m_function)(args...);
	}

	FORCEINLINE
	R call(Args... args)
	{
		return (*m_function)(args...);
	}

private:
	function_type m_function{ nullptr };
};

#include "Once.hpp"

template <class _Fty, class _GetFuncionTrait = GetFunctionChecked>
class LazyNtFunction 
{
	static_assert(_Always_false<_Fty>, "invalid generic parameter _Fty");
};

/// <summary>
/// Initialize a system routine when it is first called
/// </summary>
/// <typeparam name="_Fty">function prototype</typeparam>
template <class R, class... Args>
class LazyNtFunction<R(*)(Args...)>
	: public NtFunction<R(*)(Args...)>
{
public:
	LazyNtFunction(const WCHAR* Name) : m_name(Name) {}

	inline R operator () (Args... args) const
	{
		// call Init() only once and wait until it completed
		m_once.CallOnceAndWait([this]() {
			this->Init(m_name);
			});

		return this->call(args...);
	}
	
	inline R operator () (Args... args)
	{
		m_once.CallOnceAndWait([this]() {
			this->Init(m_name);
			});

		return this->call(args...);
	}

private:
	Once m_once;
	const WCHAR* m_name{nullptr};
};