#pragma once

#include <atomic>
#include <chrono>
#include <concurrent_priority_queue.h>
#include "Common.h"

enum TIMER_EVENT_TYPE { EV_NPC_MOVE };

struct TimerEvent {
	int obj_id;
	std::chrono::system_clock::time_point wakeup_time;
	TIMER_EVENT_TYPE event_id;

	bool operator<(const TimerEvent& other) const
	{
		return wakeup_time > other.wakeup_time;
	}
};

class TimerManager {
public:
	void add_event(int obj_id, TIMER_EVENT_TYPE ev, std::chrono::milliseconds delay);
	void run(HANDLE iocp);    // 타이머 스레드 본체
	void stop() { m_running = false; }

private:
	concurrency::concurrent_priority_queue<TimerEvent> m_queue;
	std::atomic<bool> m_running{ true };
};
