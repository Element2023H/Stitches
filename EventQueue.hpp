#pragma once
#include <ntifs.h>
#include "Common.h"

// 参考 openedr
constexpr size_t c_nAllocTag = 'gTvE';
constexpr NTSTATUS STATUS_QUEUE_LIMIT_EXEEDED = 0x4F000001;

class EventQueue
{
public:
	EventQueue() = default;
	~EventQueue() = default;

	/// <summary>
	/// 初始化
	/// </summary>
	VOID
		Initialize()
	{
		RtlZeroMemory(this, sizeof(*this));
		ExInitializeFastMutex(&m_fastMutex);
	}

	/// <summary>
	/// 判断队列是否是空
	/// </summary>
	/// <returns></returns>
	BOOLEAN
		IsEmpty()
	{
		ExAcquireFastMutex(&m_fastMutex);

		const BOOLEAN bEmpty = (m_nBeginIndex == m_nEndIndex);

		ExReleaseFastMutex(&m_fastMutex);


		return bEmpty;
	}

	/// <summary>
	/// 设置内存限制
	/// </summary>
	/// <param name="nLimit"></param>
	VOID
		SetMemoryLimit(const size_t nLimit)
	{
		m_nMemoryLimit = nLimit;
	}

	/// <summary>
	/// 清空队列
	/// </summary>
	VOID
		Finalize()
	{
		ExAcquireFastMutex(&m_fastMutex);
		__try
		{
			while (PopFrontInternal())
			{
			}
		}
		__finally
		{
			ExReleaseFastMutex(&m_fastMutex);
		}
	}

	// 保存事件内容到消息队列中
	NTSTATUS
		Pushback(const EventData& item)
	{
		ExAcquireFastMutex(&m_fastMutex);

		__try
		{
			const size_t newEndIndex = GetNextIndex(m_nEndIndex);
			if (newEndIndex == m_nBeginIndex)
			{
				LogLimitExceeded();
				return STATUS_QUEUE_LIMIT_EXEEDED;
			}

			EventData newItem{};
			auto ns = AllocateItem(item, newItem);
			if (ns == STATUS_QUEUE_LIMIT_EXEEDED)
			{
				LogLimitExceeded();
				return STATUS_QUEUE_LIMIT_EXEEDED;
			}

			if (!NT_SUCCESS(ns))
			{
				return ns;
			}

			m_EventQueue[m_nEndIndex] = newItem;
			m_nEndIndex = newEndIndex;

		}
		__finally
		{
			ExReleaseFastMutex(&m_fastMutex);
		}


		return STATUS_SUCCESS;
	}

	/// <summary>
	/// 获取第一个节点
	/// </summary>
	/// <param name="item"></param>
	/// <returns></returns>
	BOOLEAN
		GetFront(EventData& item)
	{
		ExAcquireFastMutex(&m_fastMutex);

		__try
		{
			if (m_nBeginIndex == m_nEndIndex)
			{
				return FALSE;
			}

			item = m_EventQueue[m_nBeginIndex];
		}
		__finally
		{
			ExReleaseFastMutex(&m_fastMutex);
		}

		return TRUE;
	}

	/// <summary>
	/// 清空第一个节点数据
	/// </summary>
	VOID
		PopFront()
	{
		ExAcquireFastMutex(&m_fastMutex);

		__try
		{
			(VOID)PopFrontInternal();
		}
		__finally
		{
			ExReleaseFastMutex(&m_fastMutex);
		}
	}


private:
	static constexpr size_t c_maxMessageCount = 0x4000;		// 最大的消息队列长度
	EventData m_EventQueue[c_maxMessageCount];				// 消息队列

	size_t m_nBeginIndex;		// 开始索引
	size_t m_nEndIndex;			// 结束索引

	size_t m_nTotalDataSize;	// 总数据长度
	size_t m_nMemoryLimit;		// 内存限制

	BOOLEAN m_bNoLogLimitExceeded;	// 是否超过日志限制
	FAST_MUTEX m_fastMutex;			// 同步快速互斥锁


	// 设置超过日志标志
	inline void LogLimitExceeded()
	{
		if (m_bNoLogLimitExceeded)
		{
			return;
		}

		m_bNoLogLimitExceeded = TRUE;
	}

	// 获取下一个索引
	inline size_t GetNextIndex(size_t index) noexcept
	{
		return (index + 1) % c_maxMessageCount;
	}


	// 申请/拷贝一个对象
	inline
		NTSTATUS
		AllocateItem(
			const EventData& src,	// 原始
			EventData& newItem)		// 新申请，用于保存
	{
		EventData newEventData{};

		// 创建一个空内容的
		if (!src.pDataBuffer || 0 == src.uDataLength)
		{
			newItem = newEventData;
			return STATUS_SUCCESS;
		}

		// 超出限制长度了
		if (m_nMemoryLimit != 0 &&
			(m_nTotalDataSize + src.uDataLength > m_nMemoryLimit))
		{
			return STATUS_QUEUE_LIMIT_EXEEDED;
		}

		PVOID pNewBuffer = ExAllocatePoolWithTag(PagedPool, src.uDataLength, c_nAllocTag);
		if (!pNewBuffer)
		{
			return STATUS_NO_MEMORY;
		}

		// 增加总数据长度
		m_nTotalDataSize += src.uDataLength;

		newEventData.uDataLength = src.uDataLength;
		newEventData.pDataBuffer = pNewBuffer;

		RtlCopyMemory(pNewBuffer, src.pDataBuffer, src.uDataLength);

		newItem = newEventData;

		return STATUS_SUCCESS;
	}

	// 释放实例
	VOID
		FreeItem(EventData& eventData)
	{
		if (nullptr == eventData.pDataBuffer)
		{
			return;
		}

		m_nTotalDataSize -= eventData.uDataLength;

		ExFreePoolWithTag(eventData.pDataBuffer, c_nAllocTag);
		eventData.pDataBuffer = nullptr;
	}


	// 清除第一个节点，外部应该使用锁
	BOOLEAN
		PopFrontInternal()
	{
		// 判断内容是否为空
		if (m_nBeginIndex == m_nEndIndex)
		{
			return FALSE;
		}

		EventData eventData = m_EventQueue[m_nBeginIndex];

		// 释放内存
		FreeItem(eventData);

		// 移动位置
		m_nBeginIndex = GetNextIndex(m_nBeginIndex);

		m_bNoLogLimitExceeded = FALSE;


		return TRUE;
	}



};
