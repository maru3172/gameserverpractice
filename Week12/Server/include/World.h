#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <concurrent_priority_queue.h>
#include <concurrent_queue.h>
#include "Common.h"
#include "SectorManager.h"
#include "Player.h"
#include "Npc.h"

struct TimerEvent {
	int obj_id;
	std::chrono::system_clock::time_point wakeup_time;
	bool operator<(const TimerEvent& other) const { return wakeup_time > other.wakeup_time; }
};

// 게임 월드: 플레이어/NPC 저장소, 섹터, 타이머, 접속 수락을 담당한다.
class World {
public:
	static World& instance();

	void initialize(asio::io_context& io, unsigned short port);
	void run();

	asio::io_context& io() { return *m_io; }
	SectorManager& sectors() { return m_sectors; }

	std::shared_ptr<GameObject> get_object(int id);
	std::shared_ptr<Player> get_player(int id);
	Npc* get_npc(int id);

	void enter_world(const std::shared_ptr<Player>& player);
	void update_view(const std::shared_ptr<Player>& player);
	void disconnect(int player_id);

	void add_timer(int obj_id, std::chrono::milliseconds delay);

private:
	World() = default;
	void initialize_npcs();
	void do_accept();
	void timer_loop();

	asio::io_context* m_io = nullptr;
	std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;

	std::unique_ptr<std::atomic<std::shared_ptr<Player>>[]> m_players;
	std::vector<std::unique_ptr<Npc>> m_npcs;
	concurrency::concurrent_queue<int> m_free_ids;

	SectorManager m_sectors;
	concurrency::concurrent_priority_queue<TimerEvent> m_timer_queue;
};
