#pragma once
#include "EXP_OVER.h"

class SESSION
{
public:
	SESSION();
	SESSION(SOCKET s, int id);
	~SESSION();

	bool can_see(short x, short y) const;
	bool can_send() const;

	void do_recv(); // 수신 함수
	void do_send(int num_bytes, char* mess); // 송신 함수
	void send_login_success();
	void send_avatar_info();

	void send_add_object(int object_id);
	void send_remove_object(int object_id);
	void send_move_object(int object_id);
	bool process_packet(unsigned char* p);
	void do_move(DIRECTION dir);
	void do_random_move();
	void wake_up();

	SOCKET m_client;
	int m_id;
	CL_STATE m_state;
	EXP_OVER m_recv_over;
	int m_prev_recv;
	char m_username[MAX_NAME_LEN];
	short m_x, m_y;
	int m_move_time;
	bool m_is_npc;
	std::atomic<bool> m_active_npc;
	std::chrono::system_clock::time_point m_last_npc_move_time;
	std::unordered_set<int> m_visible_objects;
	std::mutex m_visible_mutex;
};

void add_object_to_viewer_if_needed(std::shared_ptr<SESSION> viewer, int object_id);
void update_player_view(int player_id);

void send_login_fail(SOCKET client, const char* message);
int get_new_player_id();

void process_npc_move(int npc_id);

void disconnect(int key);

std::shared_ptr<SESSION> get_session(int id);