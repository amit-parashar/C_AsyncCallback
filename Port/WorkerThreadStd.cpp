#include "WorkerThreadStd.h"
#include "ThreadMsg.h"
#include "Fault.h"

using namespace std;

#define MSG_DISPATCH_DELEGATE	1
#define MSG_EXIT_THREAD			2

static WorkerThread workerThread1("Thread1");
static WorkerThread workerThread2("Thread2");

//----------------------------------------------------------------------------
// CreateThreads
//----------------------------------------------------------------------------
extern "C" void CreateThreads(void)
{
    workerThread1.CreateThread();
    workerThread2.CreateThread();
}

//----------------------------------------------------------------------------
// DispatchCallbackThread1
//----------------------------------------------------------------------------
extern "C" BOOL DispatchCallbackThread1(const CB_CallbackMsg* cbMsg)
{
    workerThread1.DispatchCallback(cbMsg);
    return TRUE;
}

//----------------------------------------------------------------------------
// DispatchCallbackThread2
//----------------------------------------------------------------------------
extern "C" BOOL DispatchCallbackThread2(const CB_CallbackMsg* cbMsg)
{
    workerThread2.DispatchCallback(cbMsg);
    return TRUE;
}

//----------------------------------------------------------------------------
// WorkerThread
//----------------------------------------------------------------------------
WorkerThread::WorkerThread(const CHAR* threadName) : m_thread(0), THREAD_NAME(threadName)
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
		m_thread = new thread(&WorkerThread::Process, this);
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
// DispatchCallback
//----------------------------------------------------------------------------
void WorkerThread::DispatchCallback(const CB_CallbackMsg* msg)
{
	ASSERT_TRUE(m_thread);

	// Create a new ThreadMsg
	ThreadMsg* threadMsg = new ThreadMsg(MSG_DISPATCH_DELEGATE, msg);

	// Add dispatch delegate msg to queue and notify worker thread
	std::unique_lock<std::mutex> lk(m_mutex);
	m_queue.push(threadMsg);
	m_cv.notify_one();
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void WorkerThread::Process()
{
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

    			// Convert the ThreadMsg void* data back to a CB_CallbackMsg* 
                const CB_CallbackMsg* callbackMsg = static_cast<const CB_CallbackMsg*>(msg->GetData());

				// Invoke the callback on the target thread
                CB_TargetInvoke(callbackMsg);

				// Delete dynamic data passed through message queue
				delete msg;
				break;
			}

			case MSG_EXIT_THREAD:
			{
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

