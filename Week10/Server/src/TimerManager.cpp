#include "TimerManager.h"
#include <thread>

using namespace std::chrono;

void TimerManager::add_event(int obj_id, TIMER_EVENT_TYPE ev, milliseconds delay)
{
	m_queue.push(TimerEvent{ obj_id, system_clock::now() + delay, ev });
}

void TimerManager::run(HANDLE iocp)
{
	while (m_running) {
		TimerEvent ev;
		if (!m_queue.try_pop(ev)) {
			std::this_thread::sleep_for(1ms);
			continue;
		}
		if (ev.wakeup_time > system_clock::now()) {
			m_queue.push(ev);
			std::this_thread::sleep_for(1ms);
			continue;
		}

		switch (ev.event_id) {
		case EV_NPC_MOVE: {
			EXP_OVER* over = new EXP_OVER(IO_NPC_MOVE);
			PostQueuedCompletionStatus(iocp, 1, ev.obj_id, &over->m_over);
			break;
		}
		}
	}
}
