#pragma once
#include <ntifs.h>

template <class...> struct Always_false { static constexpr bool value = false; };

template <class... Args>
using _Always_false = typename Always_false<Args...>::value;

template <class _Fty>
class NtFunction
{
	static_assert(_Always_false<_Fty>, "invalid generic parameter _Fty");
};

/// <summary>
/// zero-cost wrapper for calling exported function from ntoskrnl.exe
/// it will bugcheck if failed to get the system routine address
/// </summary>
/// <typeparam name="R">return value type of the target function</typeparam>
/// <typeparam name="...Args">arguments type of the target function</typeparam>
template <class R, class... Args>
class NtFunction<R(*)(Args...)>
{
	using function_type = R(NTAPI*)(Args...);
public:
	static NtFunction Force(const WCHAR* Name)
	{
		UNICODE_STRING FuncName;

		RtlInitUnicodeString(&FuncName, Name);

		auto FuncPtr = MmGetSystemRoutineAddress(&FuncName);

		return NtFunction{ reinterpret_cast<function_type>(FuncPtr) };
	}

	void Init(const WCHAR* Name)
	{
		UNICODE_STRING FuncName;

		RtlInitUnicodeString(&FuncName, Name);

		auto FuncPtr = MmGetSystemRoutineAddress(&FuncName);

		this->m_function = reinterpret_cast<function_type>(FuncPtr);
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

protected:
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

template <class _Fty> class LazyNtFunction {};

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