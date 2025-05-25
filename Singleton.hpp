#pragma once
#include "New.hpp"
#include "Once.hpp"

template<typename T>
class Singleton
{
protected:
	Singleton() = default;
	~Singleton() = default;

	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(const Singleton&) = delete;

private:
	static Once _Once;
	static T* _Instance;
public:
	static T* getInstance()
	{
		_Once.CallOnceAndWait([]() {
			auto pObject = new(NonPagedPoolNx) T;
			_Instance = pObject;
			});

		return _Instance;
	}
};

template<typename T>
T* Singleton<T>::_Instance;

template<typename T>
Once Singleton<T>::_Once;
