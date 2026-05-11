#pragma once
#include "OVER_EXP.h"

class NPC_Session
{
public:
	NPC_Session();

	~NPC_Session() {}

	void do_recv();
	void do_send(void* packet);
	void send_login_info_packet();
	void send_move_packet(int c_id);
	void send_add_player_packet(int c_id);
	void send_chat_packet(int c_id, const char* mess);
	void send_remove_player_packet(int c_id);

	std::mutex _s_lock;
	S_STATE _state;
	int _id;
	SOCKET _socket;
	short	x, y;
	char	_name[NAME_SIZE];
	int		_prev_remain;
	std::unordered_set <int> _view_list;
	std::mutex	_vl;
	int		last_move_time;

private:
	OVER_EXP _recv_over;
};
