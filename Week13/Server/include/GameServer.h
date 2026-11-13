#pragma once

#include <atomic>
#include <memory>
#include <concurrent_queue.h>
#include "Common.h"
#include "Session.h"
#include "SectorManager.h"

// Protobuf 패킷을 사용하는 IOCP 서버
class GameServer {
public:
	bool initialize(unsigned short port, int num_workers);
	void run();     // accept 루프 (메인 스레드)

private:
	std::shared_ptr<Session> get_session(int id);
	int new_session_id();
	void release_session(int id);

	void worker_thread();
	void on_recv(int id, DWORD num_bytes);
	bool process_packet(Session& session, unsigned char type, const unsigned char* data, int size);
	void process_login(Session& session, const std::string& name);
	void process_move(Session& session, int dir);
	void disconnect(int id);

	std::unique_ptr<std::atomic<std::shared_ptr<Session>>[]> m_sessions =
		std::make_unique<std::atomic<std::shared_ptr<Session>>[]>(NPC_ID_START);
	concurrency::concurrent_queue<int> m_free_ids;
	std::atomic<int> m_next_id{ 0 };

	SectorManager m_sectors;
	SOCKET m_listen = INVALID_SOCKET;
	HANDLE m_iocp = nullptr;
	int m_num_workers = 4;
};
