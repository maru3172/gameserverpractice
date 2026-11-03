#pragma once
#include "EXP_OVER.h"

class SESSION
{
public:
	SESSION();
	SESSION(SOCKET s, int id);
	~SESSION();

	bool is_npc() const;
	bool is_player() const;
	bool is_visible(short x, short y);

	void do_recv(); // 수신 함수
	void do_send(int num_bytes, char* mess); // 송신 함수

	void send_avatar_info();
	void send_move_packet(int mover);
	void send_add_player(int player_id);
	void send_login_success();
	void send_remove_player(int player_id);
	void send_chat(int player_id, char* mess);

	bool process_packet(unsigned char* p);
	void do_random_move();
	void move_notice(int mover);

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
	std::atomic<bool> _active_npc;
	std::chrono::system_clock::time_point npc_last_move_time;
	lua_State* L = nullptr;
	std::mutex m_lua_mutex;
};

void send_login_fail(SOCKET client, const char* message);
void disconnect(int key);

int API_SendMessage(lua_State* L);
int API_get_y(lua_State* L);
int API_get_x(lua_State* L);
void initialize_npcs();