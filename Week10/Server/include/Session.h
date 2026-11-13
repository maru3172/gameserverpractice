#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include "Common.h"

class Session {
public:
	Session();                       // NPC
	Session(SOCKET s, int id);       // 플레이어
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	bool can_see(short x, short y) const;
	bool can_send() const;
	bool is_visible(int object_id);

	bool do_recv();
	void do_send(int num_bytes, const void* data);
	void close_socket();

	void send_login_success();
	void send_login_fail();
	void send_avatar_info();
	void send_add_object(int object_id);
	void send_remove_object(int object_id);
	void send_move_object(int object_id);

	bool process_packet(unsigned char* p);
	void do_move(DIRECTION dir);
	void do_random_move();
	void wake_up();

	std::unordered_set<int> copy_view_list();
	std::unordered_set<int> take_view_list();

public:
	SOCKET m_client;
	int m_id;
	int m_db_user_id;
	std::atomic<CL_STATE> m_state;
	EXP_OVER m_recv_over;
	int m_prev_recv;
	char m_username[MAX_NAME_LEN];
	short m_x, m_y;
	int m_move_time;
	bool m_is_npc;
	std::atomic<bool> m_active_npc;

private:
	std::unordered_set<int> m_visible_objects;
	std::mutex m_visible_mutex;
};
