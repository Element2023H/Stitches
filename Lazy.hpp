#pragma once
#include "Once.hpp"
#include "New.hpp"
#include "Traits.hpp"

using namespace traits;

constexpr ULONG LAZY_INSTANCE_MEM = 'mmyL';

/// <summary>
/// A Lazy instance model for static singleton types, it initialize T when it is first accessed
/// </summary>
/// <typeparam name="T"></typeparam>
template <typename T, POOL_TYPE PoolType = NonPagedPoolNx>
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
		if (_Once.GetState() != state::Completed)
			KeBugCheck(MEMORY_MANAGEMENT);

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
		return get();
	}

	FORCEINLINE const T* operator -> () const
	{
		return get();
	}

	FORCEINLINE const T* get() const
	{
		return ForceDefault();
	}

	FORCEINLINE T* get()
	{
		return ForceDefault();
	}

	FORCEINLINE const T& operator * () const
	{
		return *this->get();
	}

	FORCEINLINE T& operator * ()
	{
		return *this->get();
	}

private:
	template <typename = traits::enable_if_t<traits::is_default_constructable_v<T>>>
	FORCEINLINE T* ForceDefault()
	{
		_Once.CallOnceAndWait([]() {
			_Instance = new(PoolType) T;
			});

		return _Instance;
	}
};

template <typename T, POOL_TYPE PoolType>
T* LazyInstance<T, PoolType>::_Instance;

template <typename T, POOL_TYPE PoolType>
Once LazyInstance<T, PoolType>::_Once;
