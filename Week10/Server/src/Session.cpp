#include "Session.h"
#include "GameServer.h"
#include <iostream>

using namespace std::chrono;

Session::Session()
	: m_client(INVALID_SOCKET), m_id(-1), m_db_user_id(-1), m_state(CS_FREE), m_prev_recv(0),
	m_x(0), m_y(0), m_move_time(0), m_is_npc(true), m_active_npc(false)
{
	m_username[0] = 0;
}

Session::Session(SOCKET s, int id)
	: m_client(s), m_id(id), m_db_user_id(-1), m_state(CS_CONNECT), m_prev_recv(0),
	m_move_time(0), m_is_npc(false), m_active_npc(false)
{
	m_x = static_cast<short>(rand() % WORLD_WIDTH);
	m_y = static_cast<short>(rand() % WORLD_HEIGHT);
	m_username[0] = 0;
}

Session::~Session()
{
	close_socket();
}

void Session::close_socket()
{
	SOCKET s = m_client;
	m_client = INVALID_SOCKET;
	if (s != INVALID_SOCKET)
		closesocket(s);
}

bool Session::can_see(short x, short y) const
{
	return abs(m_x - x) <= VIEW_RANGE && abs(m_y - y) <= VIEW_RANGE;
}

bool Session::can_send() const
{
	return is_pc(m_id) && m_client != INVALID_SOCKET;
}

bool Session::is_visible(int object_id)
{
	std::lock_guard<std::mutex> lock(m_visible_mutex);
	return m_visible_objects.count(object_id) > 0;
}

std::unordered_set<int> Session::copy_view_list()
{
	std::lock_guard<std::mutex> lock(m_visible_mutex);
	return m_visible_objects;
}

std::unordered_set<int> Session::take_view_list()
{
	std::lock_guard<std::mutex> lock(m_visible_mutex);
	std::unordered_set<int> result;
	result.swap(m_visible_objects);
	return result;
}

bool Session::do_recv()
{
	DWORD recv_flag = 0;
	ZeroMemory(&m_recv_over.m_over, sizeof(m_recv_over.m_over));
	m_recv_over.m_wsa.len = BUF_SIZE - m_prev_recv;
	m_recv_over.m_wsa.buf = m_recv_over.m_buff + m_prev_recv;
	int ret = WSARecv(m_client, &m_recv_over.m_wsa, 1, 0, &recv_flag, &m_recv_over.m_over, nullptr);
	if (ret == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) return false;
	}
	return true;
}

void Session::do_send(int num_bytes, const void* data)
{
	if (!can_send()) return;
	if (num_bytes <= 0 || num_bytes > BUF_SIZE) return;

	EXP_OVER* over = new EXP_OVER(IO_SEND);
	over->m_wsa.len = num_bytes;
	memcpy(over->m_buff, data, num_bytes);
	int ret = WSASend(m_client, &over->m_wsa, 1, 0, 0, &over->m_over, nullptr);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		delete over;    // 완료 통지가 오지 않으므로 여기서 해제
}

void Session::send_login_success()
{
	S2C_LoginResult packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = true;
	strcpy_s(packet.message, "Login successful.");
	do_send(packet.size, &packet);
}

void Session::send_login_fail()
{
	S2C_LoginResult packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strcpy_s(packet.message, "Login failed.");
	do_send(packet.size, &packet);
}

void Session::send_avatar_info()
{
	S2C_AvatarInfo packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_AVATAR_INFO;
	packet.playerId = m_id;
	packet.x = m_x;
	packet.y = m_y;
	do_send(packet.size, &packet);
}

void Session::send_add_object(int object_id)
{
	if (!can_send()) return;

	auto obj = GameServer::instance().get_session(object_id);
	if (nullptr == obj || obj->m_state != CS_PLAYING) return;

	{
		std::lock_guard<std::mutex> lock(m_visible_mutex);
		if (!m_visible_objects.insert(object_id).second) return;
	}

	S2C_AddPlayer packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_ADD_PLAYER;
	packet.playerId = object_id;
	strcpy_s(packet.username, obj->m_username);
	packet.x = obj->m_x;
	packet.y = obj->m_y;
	do_send(packet.size, &packet);
}

void Session::send_remove_object(int object_id)
{
	if (!can_send()) return;

	{
		std::lock_guard<std::mutex> lock(m_visible_mutex);
		if (m_visible_objects.erase(object_id) == 0) return;
	}

	S2C_RemovePlayer packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_REMOVE_PLAYER;
	packet.playerId = object_id;
	do_send(packet.size, &packet);
}

void Session::send_move_object(int object_id)
{
	if (!can_send()) return;

	auto obj = GameServer::instance().get_session(object_id);
	if (nullptr == obj || obj->m_state != CS_PLAYING) return;

	S2C_MovePlayer packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_MOVE_PLAYER;
	packet.playerId = object_id;
	packet.x = obj->m_x;
	packet.y = obj->m_y;
	packet.move_time = obj->m_move_time;
	do_send(packet.size, &packet);
}

bool Session::process_packet(unsigned char* p)
{
	PACKET_TYPE type = *reinterpret_cast<PACKET_TYPE*>(&p[1]);
	switch (type) {
	case C2S_LOGIN: {
		if (p[0] < sizeof(C2S_Login)) return false;
		auto* packet = reinterpret_cast<C2S_Login*>(p);
		CL_STATE expected = CS_CONNECT;
		if (!m_state.compare_exchange_strong(expected, CS_DB_WAIT)) return false;
		GameServer::instance().db().request_login(m_id, packet->user_id);
		return true;
	}
	case C2S_MOVE: {
		if (p[0] < sizeof(C2S_Move)) return false;
		if (m_state != CS_PLAYING) return true;    // 로그인 완료 전 이동은 무시
		auto* packet = reinterpret_cast<C2S_Move*>(p);
		m_move_time = packet->move_time;
		do_move(packet->dir);
		return true;
	}
	default:
		std::cout << "Unknown packet type received from player[" << m_id << "].\n";
		return false;
	}
}

void Session::do_move(DIRECTION dir)
{
	short old_x = m_x;
	short old_y = m_y;

	switch (dir) {
	case UP:    if (m_y > 0) --m_y; break;
	case DOWN:  if (m_y < WORLD_HEIGHT - 1) ++m_y; break;
	case LEFT:  if (m_x > 0) --m_x; break;
	case RIGHT: if (m_x < WORLD_WIDTH - 1) ++m_x; break;
	default: return;
	}

	auto& server = GameServer::instance();
	server.sectors().update_object(m_id, old_x, old_y, m_x, m_y);
	server.update_player_view(m_id);
	if (m_db_user_id != -1)
		server.db().request_save_position(m_db_user_id, m_x, m_y);
}

void Session::do_random_move()
{
	auto& server = GameServer::instance();
	short old_x = m_x;
	short old_y = m_y;

	std::unordered_set<int> old_viewers;
	for (int object_id : server.sectors().get_objects_in_adjacent_sectors(old_x, old_y)) {
		if (!is_pc(object_id)) continue;
		auto player = server.get_session(object_id);
		if (nullptr == player || !player->can_send()) continue;
		if (player->can_see(old_x, old_y))
			old_viewers.insert(object_id);
	}

	switch (rand() % 4) {
	case 0: if (m_y > 0) --m_y; break;
	case 1: if (m_y < WORLD_HEIGHT - 1) ++m_y; break;
	case 2: if (m_x > 0) --m_x; break;
	case 3: if (m_x < WORLD_WIDTH - 1) ++m_x; break;
	}

	server.sectors().update_object(m_id, old_x, old_y, m_x, m_y);

	std::unordered_set<int> new_viewers;
	for (int object_id : server.sectors().get_objects_in_adjacent_sectors(m_x, m_y)) {
		if (!is_pc(object_id)) continue;
		auto player = server.get_session(object_id);
		if (nullptr == player || !player->can_send()) continue;
		if (!player->can_see(m_x, m_y)) continue;
		new_viewers.insert(object_id);

		if (player->is_visible(m_id))
			player->send_move_object(m_id);
		else
			player->send_add_object(m_id);
	}

	for (int player_id : old_viewers) {
		if (new_viewers.count(player_id) != 0) continue;
		auto player = server.get_session(player_id);
		if (nullptr != player)
			player->send_remove_object(m_id);
	}
}

void Session::wake_up()
{
	bool expected = false;
	if (!m_active_npc.compare_exchange_strong(expected, true))
		return;
	GameServer::instance().timer().add_event(m_id, EV_NPC_MOVE, milliseconds(MOVE_COOL_TIME));
}
