#include "DelegateOpt.h"
#if USE_STD_THREADS

#include "WorkerThreadStd.h"
#include "ThreadMsg.h"
#include "Timer.h"

#ifdef WIN32
#include <Windows.h>
#endif

using namespace std;
using namespace DelegateLib;

#define MSG_DISPATCH_DELEGATE	1
#define MSG_EXIT_THREAD			2
#define MSG_TIMER				3

//----------------------------------------------------------------------------
// WorkerThread
//----------------------------------------------------------------------------
WorkerThread::WorkerThread(const CHAR* threadName) : m_thread(0), m_timerExit(false), THREAD_NAME(threadName)
{
}

//----------------------------------------------------------------------------
// ~WorkerThread
//----------------------------------------------------------------------------
WorkerThread::~WorkerThread()
{
	ExitThread();
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
BOOL WorkerThread::CreateThread()
{
	if (!m_thread)
	{
		m_thread = new thread(&WorkerThread::Process, this);

#ifdef WIN32
		// Get the thread's native Windows handle
		auto handle = m_thread->native_handle();

		// Set the thread name so it shows in the Visual Studio Debug Location toolbar
		std::wstring wstr(THREAD_NAME.begin(), THREAD_NAME.end());
		HRESULT hr = SetThreadDescription(handle, wstr.c_str());
		if (FAILED(hr))
		{
			// Handle error if needed
		}
#endif
	}
	return TRUE;
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
std::thread::id WorkerThread::GetThreadId()
{
	ASSERT_TRUE(m_thread != 0);
	return m_thread->get_id();
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
std::thread::id WorkerThread::GetCurrentThreadId()
{
	return this_thread::get_id();
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void WorkerThread::ExitThread()
{
	if (!m_thread)
		return;

	// Create a new ThreadMsg
	ThreadMsg* threadMsg = new ThreadMsg(MSG_EXIT_THREAD, 0);

	// Put exit thread message into the queue
	{
		lock_guard<mutex> lock(m_mutex);
		m_queue.push(threadMsg);
		m_cv.notify_one();
	}

	m_thread->join();
	delete m_thread;
	m_thread = 0;
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void WorkerThread::DispatchDelegate(DelegateLib::DelegateMsgBase* msg)
{
	ASSERT_TRUE(m_thread);

	ThreadMsg* threadMsg = new ThreadMsg(MSG_DISPATCH_DELEGATE, msg);

	// Add dispatch delegate msg to queue and notify worker thread
	std::unique_lock<std::mutex> lk(m_mutex);
	m_queue.push(threadMsg);
	m_cv.notify_one();
}

//----------------------------------------------------------------------------
// TimerThread
//----------------------------------------------------------------------------
void WorkerThread::TimerThread()
{
	while (!m_timerExit)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		ThreadMsg* threadMsg = new ThreadMsg(MSG_TIMER, 0);

		// Add timer msg to queue and notify worker thread
		std::unique_lock<std::mutex> lk(m_mutex);
		m_queue.push(threadMsg);
		m_cv.notify_one();
	}
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void WorkerThread::Process()
{
	m_timerExit = false;
	std::thread timerThread(&WorkerThread::TimerThread, this);

	while (1)
	{
		ThreadMsg* msg = 0;
		{
			// Wait for a message to be added to the queue
			std::unique_lock<std::mutex> lk(m_mutex);
			while (m_queue.empty())
				m_cv.wait(lk);

			if (m_queue.empty())
				continue;

			msg = m_queue.front();
			m_queue.pop();
		}

		switch (msg->GetId())
		{
			case MSG_DISPATCH_DELEGATE:
			{
				ASSERT_TRUE(msg->GetData() != NULL);

				// Convert the ThreadMsg void* data back to a DelegateMsg* 
				DelegateMsgBase* delegateMsg = static_cast<DelegateMsgBase*>(msg->GetData());

				// Invoke the callback on the target thread
				delegateMsg->GetDelegateInvoker()->DelegateInvoke(&delegateMsg);

				// Delete dynamic data passed through message queue
				delete msg;
				break;
			}

			case MSG_TIMER:
				Timer::ProcessTimers();
				delete msg;
				break;

			case MSG_EXIT_THREAD:
			{
				m_timerExit = true;
				timerThread.join();

				delete msg;
				std::unique_lock<std::mutex> lk(m_mutex);
				while (!m_queue.empty())
				{
					msg = m_queue.front();
					m_queue.pop();
					delete msg;
				}
				return;
			}

			default:
				ASSERT();
		}
	}
}

#endif
