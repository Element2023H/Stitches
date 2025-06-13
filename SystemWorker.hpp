#pragma once
#include <ntifs.h>

// 参考 openedr

enum class DoWorkResult
{
	HasWork,		// 
	NoWork,			// 需要通知调用
	Terminate		// 已经结束
};

class SystemWorker 
{
public:
	typedef DoWorkResult(*PFnDoWork)(void* pContext);

	/// <summary>
	/// 初始化
	/// </summary>
	/// <param name="pfnWork"></param>
	/// <param name="pContext"></param>
	/// <param name="nWalkupInterval"></param>
	/// <returns></returns>
	bool
		Initialize(
			PFnDoWork pfnWork,
			void* pContext,
			ULONG nWalkupInterval)
	{
		RtlZeroMemory(this, sizeof(*this));

		if (nullptr == pfnWork)
		{
			return false;
		}

		// 初始化成员变量
		m_pFnDoWork = pfnWork;
		m_pContext = pContext;
		m_nWakeupIntervcal = nWalkupInterval;

		ExInitializeFastMutex(&m_fastMutex);
		KeInitializeEvent(&m_event, NotificationEvent, FALSE);


		HANDLE hThread = nullptr;

		// 创建系统线程
		NTSTATUS ns = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS,
			NULL, NULL, NULL,
			(PKSTART_ROUTINE)&run,		// 线程函数体
			(PVOID)this);
		if (!NT_SUCCESS(ns))
		{
			return false;
		}

		// 设置线程对象
		ns = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL, KernelMode, &m_pThreadObject, NULL);
		ZwClose(hThread);
		if (!NT_SUCCESS(ns))
		{
			return false;
		}

		m_bIsInitialized = TRUE;
		return true;
	}

	/// <summary>
	/// 设置时间差
	/// </summary>
	/// <param name="nWakeupInterval"></param>
	VOID
		SetWakeupInterval(ULONG nWakeupInterval)
	{
		ExAcquireFastMutex(&m_fastMutex);

		m_nWakeupIntervcal = nWakeupInterval;

		ExReleaseFastMutex(&m_fastMutex);
	}

	VOID
		Finalize()
	{
		if (!m_bIsInitialized)
		{
			return;
		}

		m_bIsInitialized = FALSE;

		if (!m_pThreadObject)
		{
			return;
		}

		m_bTerminated = TRUE;

		KeSetEvent(&m_event, IO_NO_INCREMENT, FALSE);

		// 等待线程对象
		KeWaitForSingleObject(m_pThreadObject, Executive, KernelMode, FALSE, nullptr);

		ObDereferenceObject(m_pThreadObject);
		m_pThreadObject = nullptr;

	}

	/// <summary>
	/// 通知等待线程可以就绪了
	/// </summary>
	VOID
		Notify()
	{
		if (!m_bIsInitialized || m_bTerminated)
		{
			return;
		}

		KeSetEvent(&m_event, IO_NO_INCREMENT, FALSE);

	}

	/// <summary>
	/// 判断是否已经结束
	/// </summary>
	/// <returns></returns>
	BOOLEAN
		IsTerminated()
	{
		if (!m_bIsInitialized)
		{
			return TRUE;
		}

		return m_bTerminated;
	}


private:
	PVOID		m_pContext;		// 上下文
	PFnDoWork	m_pFnDoWork;	// 工作函数

	BOOLEAN		m_bIsInitialized = FALSE;	// 是否已经初始化
	BOOLEAN		m_bTerminated = FALSE;		// 是否被结束

	PVOID		m_pThreadObject;	// 线程对象
	KEVENT		m_event;			// 事件
	FAST_MUTEX  m_fastMutex;		// 同步锁（快速互斥量）

	ULONG		m_nWakeupIntervcal;	// 时间跨度

	// 线程函数
	static
		VOID
		run(PVOID pStartContext)
	{
		auto pSystemWorker = static_cast<SystemWorker*>(pStartContext);
		pSystemWorker->doProcessing();


		// 结束线程
		PsTerminateSystemThread(STATUS_SUCCESS);
	}


	/// <summary>
	/// 执行体函数实现
	/// </summary>
	void doProcessing()
	{
		DoWorkResult result = DoWorkResult::NoWork;

		while (result != DoWorkResult::Terminate)
		{
			if (result != DoWorkResult::HasWork)
			{
				LARGE_INTEGER liTimeOut{};

				PLARGE_INTEGER pLITimeout = nullptr;

				ExAcquireFastMutex(&m_fastMutex);

				if (m_nWakeupIntervcal != 0)
				{
					liTimeOut.QuadPart = (LONGLONG)m_nWakeupIntervcal * 1000 * 10 * (LONGLONG)-1;
					pLITimeout = &liTimeOut;
				}


				ExReleaseFastMutex(&m_fastMutex);

				// 等待事件对象
				auto ns = KeWaitForSingleObject(&m_event, Executive, KernelMode, FALSE, pLITimeout);
				if (!NT_SUCCESS(ns))
				{
					break;
				}

				// 清除信号状态
				KeClearEvent(&m_event);

			}

			if (m_bTerminated)
			{
				break;
			}

			// do it
			result = m_pFnDoWork(m_pContext);
		}

	}
};

