#pragma once
#include <ntifs.h>

namespace state {
	constexpr ULONG Initial = 0;
	constexpr ULONG InProgress = 1;
	constexpr ULONG Completed = 2;
	constexpr ULONG Poisoned = 3;
}

// primitive for "Execute Only Once"
class Once final
{
public:
	Once() = default;
	Once(const Once&) = delete;
	Once& operator=(const Once&) = delete;

	Once(Once&& other) noexcept
	{
		_MoveAssign(other);
	}

	Once& operator=(Once&& other) noexcept
	{
		if (this != &other)
			_MoveAssign(other);
	}

	FORCEINLINE
	ULONG GetState() const { return this->m_state; }

	FORCEINLINE
	void ForceWait() const
	{
		while (this->m_state != state::Completed)
			_mm_pause();
	}

	template <class _Fty>
	inline void CallOnce(_Fty init)
	{
		if (state::Initial ==
			_InterlockedCompareExchange(
				reinterpret_cast<volatile long*>(&this->m_state), state::InProgress, state::Initial))
		{
			init();

			_InterlockedCompareExchange(
				reinterpret_cast<volatile long*>(&this->m_state), state::Completed, state::InProgress);
		}
	}

	template <class _Fty>
	inline void CallOnceAndWait(_Fty init)
	{
		if (state::Completed == this->m_state)
			return;

		if (state::Initial ==
			_InterlockedCompareExchange(
				reinterpret_cast<volatile long*>(&this->m_state), state::InProgress, state::Initial))
		{
			init();

			_InterlockedCompareExchange(
				reinterpret_cast<volatile long*>(&this->m_state), state::Completed, state::InProgress);
		}
		else if (state::InProgress == this->m_state)
		{
			this->ForceWait();
		}
		else
		{
			KeBugCheck(MEMORY_MANAGEMENT);
		}
	}
private:
	inline void _MoveAssign(Once& other)
	{
		this->m_state = other.m_state;
		_InterlockedExchange(reinterpret_cast<volatile long*>(&other.m_state), state::Poisoned);
	}

private:
	ULONG m_state{ state::Initial };
};