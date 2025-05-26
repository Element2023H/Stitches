#pragma once
#include "Once.hpp"
#include "New.hpp"


const ULONG LAZY_INSTANCE_MEM = 'mmyL';

/// <summary>
/// A Lazy instance model for static singleton types, it initialize T when it is first accessed
/// </summary>
/// <typeparam name="T"></typeparam>
template <typename T>
class LazyInstance
{
public:
	LazyInstance() = default;
	~LazyInstance() = default;

	LazyInstance(const LazyInstance&) = delete;
	LazyInstance(LazyInstance&&) = delete;
	LazyInstance& operator=(const LazyInstance&) = delete;

private:
	static Once _Once;
	static T* _Instance;

public:
	/// <summary>
	/// initialize T using customized function
	/// </summary>
	/// <typeparam name="_Init"></typeparam>
	/// <param name="init"></param>
	/// <returns></returns>
	template <typename _Init>
	FORCEINLINE static void Force(_Init init)
	{
		_Once.CallOnceAndWait([&init]() {
			_Instance = init();
			});
	}

	FORCEINLINE static void Dispose()
	{
		delete _Instance;
		_Once.SetPoisoned();
	}

	FORCEINLINE operator bool() const
	{
		return _Instance != nullptr;
	}

	/// <summary>
	/// initialize T using default CTOR
	/// </summary>
	/// <returns></returns>
	FORCEINLINE T* operator -> ()
	{
		_Once.CallOnceAndWait([this]() {
			_Instance = this->ForceDefault();
			});

		return _Instance;
	}

	FORCEINLINE const T* operator -> () const
	{
		_Once.CallOnceAndWait([this]() {
			_Instance = this->ForceDefault();
			});

		return _Instance;
	}

private:
	FORCEINLINE T* ForceDefault()
	{
		//return reinterpret_cast<T*>(ExAllocatePoolWithTag(NonPagedPoolNx, sizeof(T), LAZY_INSTANCE_MEM));
		return new(NonPagedPoolNx) T;
	}
};

template <typename T>
T* LazyInstance<T>::_Instance;

template <typename T>
Once LazyInstance<T>::_Once;
