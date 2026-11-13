#pragma once

#include <memory>
#include <mutex>
#include "GameObject.h"

// 네트워크에 연결된 플레이어 세션 (asio 비동기 수신 + 동기 송신)
class Player : public GameObject, public std::enable_shared_from_this<Player> {
public:
	Player(asio::ip::tcp::socket socket, int id);

	void start();
	void close();

	void send_login_info();
	void send_add_object(int obj_id);
	void send_move_object(int obj_id);
	void send_remove_object(int obj_id);
	void send_chat(int from_id, const char* mess);

private:
	void do_recv();
	bool process_data(size_t num_bytes);
	bool process_packet(unsigned char* packet);
	void on_login(const CS_LOGIN_PACKET& p);
	void on_move(const CS_MOVE_PACKET& p);
	void do_send(const void* packet);

	asio::ip::tcp::socket m_socket;
	unsigned char m_recv_buff[NET_BUFF_SIZE];
	size_t m_prev_remain = 0;
	std::mutex m_send_lock;
};
