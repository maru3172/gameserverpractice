#pragma once
#include "EXP_OVER.h"

class SESSION
{
public:
	SESSION();
	SESSION(SOCKET s, int id);
	~SESSION();

	bool is_visible(short other_x, short other_y);
	void do_recv(); // 수신 함수
	void do_send(int num_bytes, char* mess); // 송신 함수
	void do_move(DIRECTION dir);

	void send_avatar_info();
	void send_move_packet(int mover);
	void send_add_player(int player_id);
	void send_login_success();
	void send_remove_player(int player_id);

	bool process_packet(unsigned char* p);

	SOCKET m_client;
	int m_id;
	CL_STATE m_state;
	EXP_OVER m_recv_over;
	int m_prev_recv;
	char m_username[MAX_NAME_LEN];
	short m_x, m_y;
	int m_move_time;
	std::unordered_set<int> m_visible_players;
	std::mutex m_visible_mutex;
};

void send_login_fail(SOCKET client, const char* message);

void disconnect(int key);
