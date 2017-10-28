#include "SelfTestEngine.h"

MulticastDelegateSafe1<const SelfTestStatus&> SelfTestEngine::StatusCallback;

//------------------------------------------------------------------------------
// GetInstance
//------------------------------------------------------------------------------
SelfTestEngine& SelfTestEngine::GetInstance()
{
	static SelfTestEngine instance;
	return instance;
}

//------------------------------------------------------------------------------
// SelfTestEngine
//------------------------------------------------------------------------------
SelfTestEngine::SelfTestEngine() :
	SelfTest(ST_MAX_STATES),
	m_thread("SelfTestEngine")
{
	// Register for callbacks when sub self-test state machines complete or fail
	m_centrifugeTest.CompletedCallback += MakeDelegate(this, &SelfTestEngine::Complete, &m_thread);
	m_centrifugeTest.FailedCallback += MakeDelegate<SelfTest>(this, &SelfTest::Cancel, &m_thread);
	m_pressureTest.CompletedCallback += MakeDelegate(this, &SelfTestEngine::Complete, &m_thread);
	m_pressureTest.FailedCallback += MakeDelegate<SelfTest>(this, &SelfTest::Cancel, &m_thread);
}

//------------------------------------------------------------------------------
// InvokeStatusCallback
//------------------------------------------------------------------------------
void SelfTestEngine::InvokeStatusCallback(std::string msg)
{
	// Client(s) registered?
	if (StatusCallback)
	{
		SelfTestStatus status;
		status.message = msg;

		// Callback registered client(s)
		StatusCallback(status);
	}
}

//------------------------------------------------------------------------------
// Start
//------------------------------------------------------------------------------
void SelfTestEngine::Start(const StartData* data)
{
	// Is the caller executing on m_thread?
    if (m_thread.GetThreadId() != WorkerThread::GetCurrentThreadId())
    {
        // Create an asynchronous delegate and reinvoke the function call on m_thread
        Delegate1<const StartData*>& delegate = MakeDelegate(this, &SelfTestEngine::Start, &m_thread);
        delegate(data);
        return;
    }

	BEGIN_TRANSITION_MAP			              			// - Current State -
		TRANSITION_MAP_ENTRY (ST_START_CENTRIFUGE_TEST)		// ST_IDLE
		TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)				// ST_COMPLETED
		TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)				// ST_FAILED
		TRANSITION_MAP_ENTRY (EVENT_IGNORED)				// ST_START_CENTRIFUGE_TEST
		TRANSITION_MAP_ENTRY (EVENT_IGNORED)				// ST_START_PRESSURE_TEST
	END_TRANSITION_MAP(data)
}

//------------------------------------------------------------------------------
// Complete
//------------------------------------------------------------------------------
void SelfTestEngine::Complete()
{
	BEGIN_TRANSITION_MAP			              			// - Current State -
		TRANSITION_MAP_ENTRY (EVENT_IGNORED)				// ST_IDLE
		TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)				// ST_COMPLETED
		TRANSITION_MAP_ENTRY (CANNOT_HAPPEN)				// ST_FAILED
		TRANSITION_MAP_ENTRY (ST_START_PRESSURE_TEST)		// ST_START_CENTRIFUGE_TEST
		TRANSITION_MAP_ENTRY (ST_COMPLETED)					// ST_START_PRESSURE_TEST
	END_TRANSITION_MAP(NULL)
}

//------------------------------------------------------------------------------
// StartCentrifugeTest
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTestEngine, StartCentrifugeTest, StartData)
{
	m_startData = *data;

	InvokeStatusCallback("SelfTestEngine::ST_CentrifugeTest");
	m_centrifugeTest.Start(&m_startData);
}

//------------------------------------------------------------------------------
// StartPressureTest
//------------------------------------------------------------------------------
STATE_DEFINE(SelfTestEngine, StartPressureTest, NoEventData)
{
	InvokeStatusCallback("SelfTestEngine::ST_PressureTest");
 	m_pressureTest.Start(&m_startData);
}

