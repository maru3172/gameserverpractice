#pragma once

#include <atomic>
#include <memory>
#include <concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include "Common.h"
#include "Session.h"
#include "SectorManager.h"
#include "TimerManager.h"
#include "DBManager.h"

// IOCP 기반 게임 서버. 세션/섹터/타이머/DB 를 소유하고 워커 스레드를 돌린다.
class GameServer {
public:
	static GameServer& instance();

	bool initialize();
	void run();

	std::shared_ptr<Session> get_session(int id);
	SectorManager& sectors() { return m_sectors; }
	TimerManager& timer() { return m_timer; }
	DBManager& db() { return m_db; }

	void update_player_view(int player_id);
	void disconnect(int key);

private:
	GameServer() = default;

	void initialize_npcs();
	void post_accept();
	int get_new_player_id();
	void release_session(int key);

	void worker_thread();
	void on_accept(EXP_OVER* over);
	void on_recv(int key, DWORD num_bytes);
	void on_db_login(int key, EXP_OVER* over);
	void on_npc_move(int npc_id);
	static void send_server_full(SOCKET client);

	tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<Session>>> m_clients;
	concurrency::concurrent_queue<int> m_free_ids;
	std::atomic<int> m_next_player_id{ 1 };

	SectorManager m_sectors;
	TimerManager m_timer;
	DBManager m_db;

	SOCKET m_listen = INVALID_SOCKET;
	HANDLE m_iocp = nullptr;
	EXP_OVER m_accept_over{ IO_ACCEPT };
};
