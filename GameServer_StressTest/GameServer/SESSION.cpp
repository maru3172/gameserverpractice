#include "pch.h"
#include "SESSION.h"
#include "SectorManager.h"

tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;

SESSION::SESSION() : m_client(INVALID_SOCKET), m_id(-1), m_state(CS_FREE), m_prev_recv(0), m_x(0),
m_y(0), m_move_time(0), m_is_npc(false), m_active_npc(false)
{
	m_username[0] = 0;
	m_recv_over.m_iotype = IO_RECV;
	m_last_npc_move_time = std::chrono::system_clock::now();
}

SESSION::SESSION(SOCKET s, int id) : m_client(s), m_id(id), m_state(CS_CONNECT), m_prev_recv(0),
m_move_time(0), m_is_npc(false), m_active_npc(false)
{
	m_state = CS_CONNECT;
	m_recv_over.m_iotype = IO_RECV;
	m_x = rand() % WORLD_WIDTH;
	m_y = rand() % WORLD_HEIGHT;
	m_prev_recv = 0;
	m_move_time = 0;
}

SESSION::~SESSION()
{
	if (m_client != INVALID_SOCKET) closesocket(m_client);
}

bool SESSION::can_see(short x, short y) const // 3D게임에선 원, 2D는 사각형, 내 시야 거리 안에 존재하는가?
{
	return abs(m_x - x) <= VIEW_RANGE && abs(m_y - y) <= VIEW_RANGE;
}

bool SESSION::can_send() const
{
	return (m_id < MAX_PLAYERS) && m_client != INVALID_SOCKET;
}

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&m_recv_over.m_over, 0, sizeof(m_recv_over.m_over));
	m_recv_over.m_wsa.len = BUF_SIZE - m_prev_recv;
	m_recv_over.m_wsa.buf = m_recv_over.m_buff + m_prev_recv;
	WSARecv(m_client, &m_recv_over.m_wsa, 1, 0, &recv_flag, &m_recv_over.m_over, nullptr);
}

void SESSION::do_send(int num_bytes, char* mess)
{
	if (!can_send()) return;

	EXP_OVER* o = new EXP_OVER(IO_SEND);
	o->m_wsa.len = num_bytes;
	memcpy(o->m_buff, mess, num_bytes);
	WSASend(m_client, &o->m_wsa, 1, 0, 0, &o->m_over, nullptr);
}

void SESSION::send_login_success()
{
	S2C_LoginResult packet;
	packet.size = sizeof(packet);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = true;
	strcpy_s(packet.message, "Login successful.");
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_avatar_info()
{
	S2C_AvatarInfo packet;
	packet.size = sizeof(packet);
	packet.type = S2C_AVATAR_INFO;
	packet.playerId = m_id;
	packet.x = m_x;
	packet.y = m_y;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::do_move(DIRECTION dir)
{
	short old_x = m_x, old_y = m_y;

	// Read중 데이터 레이스로 인해 다른 쓰레드의 수정으로 Crash 걸릴 가능성이 있음, 그래서 lock을 걸고 데이터 복사
	switch (dir) {
	case UP: if (m_y > 0) --m_y; break;
	case DOWN: if (m_y < WORLD_HEIGHT - 1) ++m_y; break;
	case LEFT: if (m_x > 0) --m_x; break;
	case RIGHT: if (m_x < WORLD_WIDTH - 1) ++m_x; break;
	}

	sector_manager.update_object_sector(m_id, old_x, old_y, m_x, m_y);
	update_player_view(m_id);
}

void SESSION::do_random_move()
{
	short old_x = m_x, old_y = m_y;
	std::unordered_set<int> old_viewers;
	for (int object_id : sector_manager.get_objects_in_adjacent_sectors(old_x, old_y)) {
		if (!is_pc(object_id)) continue;

		std::shared_ptr<SESSION> player = get_session(object_id);
		if (nullptr == player || !player->can_send()) continue;
		if (player->can_see(old_x, old_y)) old_viewers.insert(object_id);
	}

	switch (rand() % 4) {
	case 0: if (m_y > 0) --m_y; break;
	case 1: if (m_y < WORLD_HEIGHT - 1) ++m_y; break;
	case 2: if (m_x > 0) --m_x; break;
	case 3: if (m_x < WORLD_WIDTH - 1) ++m_x; break;
	}

	sector_manager.update_object_sector(m_id, old_x, old_y, m_x, m_y);
	m_last_npc_move_time = std::chrono::system_clock::now();

	std::unordered_set<int> new_viewers;
	for (int object_id : sector_manager.get_objects_in_adjacent_sectors(m_x, m_y)) {
		if (!is_pc(object_id)) continue;

		std::shared_ptr<SESSION> player = get_session(object_id);
		if (nullptr == player || !player->can_send()) continue;
		if (!player->can_see(m_x, m_y)) continue;
		new_viewers.insert(object_id);

		bool already_visible = false;
		{
			std::lock_guard<std::mutex> lock(player->m_visible_mutex);
			already_visible = player->m_visible_objects.count(m_id) > 0;
		}

		if (already_visible) player->send_move_object(m_id);
		else player->send_add_object(m_id);
	}

	for (int player_id : old_viewers) {
		if (new_viewers.count(player_id) != 0) continue;

		std::shared_ptr<SESSION> player = get_session(player_id);
		if (nullptr != player) player->send_remove_object(m_id);
	}
}

void SESSION::wake_up()
{
	bool expected = false;
	if (!m_active_npc.compare_exchange_strong(expected, true)) return;

	event_type ev;
	ev.obj_id = m_id;
	ev.event_id = EVENT_NPC_MOVE;
	ev.wakeup_time = std::chrono::system_clock::now() + std::chrono::milliseconds(MOVE_COOL_TIME);
	timer_queue.push(ev);
}

void SESSION::send_add_object(int object_id)
{
	if (!can_send()) return;

	std::shared_ptr<SESSION> obj = get_session(object_id);
	if (nullptr == obj) return;
	if (obj->m_state != CS_PLAYING) return;

	{
		std::lock_guard<std::mutex> lock(m_visible_mutex);
		if (m_visible_objects.count(object_id) > 0) return;
		m_visible_objects.insert(object_id);
	}

	S2C_AddPlayer packet;
	packet.size = sizeof(packet);
	packet.type = S2C_ADD_PLAYER;
	packet.playerId = object_id;
	strcpy_s(packet.username, obj->m_username);
	packet.x = obj->m_x;
	packet.y = obj->m_y;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_remove_object(int object_id)
{
	if (!can_send()) return;

	{
		std::lock_guard<std::mutex> lock(m_visible_mutex);
		if (m_visible_objects.count(object_id) == 0) return;
		m_visible_objects.erase(object_id);
	}

	S2C_RemovePlayer packet;
	packet.size = sizeof(packet);
	packet.type = S2C_REMOVE_PLAYER;
	packet.playerId = object_id;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_move_object(int object_id)
{
	if (!can_send()) return;

	std::shared_ptr<SESSION> obj = get_session(object_id);
	if (nullptr == obj) return;
	if (obj->m_state != CS_PLAYING) return;

	S2C_MovePlayer packet;
	packet.size = sizeof(packet);
	packet.type = S2C_MOVE_PLAYER;
	packet.playerId = object_id;
	packet.x = obj->m_x;
	packet.y = obj->m_y;
	packet.move_time = obj->m_move_time;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

bool SESSION::process_packet(unsigned char* p)
{
	// 현재 패킷이 무슨 상태인지 파악
	PACKET_TYPE type = *reinterpret_cast<PACKET_TYPE*>(&p[1]);
	switch (type)
	{
	case C2S_LOGIN:
	{
		C2S_Login* packet = reinterpret_cast<C2S_Login*>(p);
		strncpy_s(m_username, packet->username, MAX_NAME_LEN);
		m_state = CS_PLAYING;
		sector_manager.add_object_to_sector(m_id, m_x, m_y);
		send_avatar_info();
		update_player_view(m_id);
	}
	break;
	case C2S_MOVE:
	{
		C2S_Move* packet = reinterpret_cast<C2S_Move*>(p);
		m_move_time = packet->move_time;
		do_move(packet->dir);
	}
	break;
	default: // 플레이어로부터 알 수 없는 패킷이 전송됨
		std::cout << "Unknown packet type received from player[" << m_id << "].\n";
		return false;
	}
	return true;
}

void add_object_to_viewer_if_needed(std::shared_ptr<SESSION> viewer, int object_id)
{
	if (nullptr == viewer || !viewer->can_send()) return;
	viewer->send_add_object(object_id);
}

void update_player_view(int player_id)
{
	std::shared_ptr<SESSION> player = get_session(player_id);
	if (nullptr == player || !player->can_send()) return;

	std::unordered_set<int> old_view;
	{
		std::lock_guard<std::mutex> lock(player->m_visible_mutex);
		old_view = player->m_visible_objects;
	}

	std::unordered_set<int> new_view;
	for (int object_id : sector_manager.get_objects_in_adjacent_sectors(player->m_x, player->m_y)) {
		if (object_id == player_id) continue;

		std::shared_ptr<SESSION> obj = get_session(object_id);
		if (nullptr == obj) continue;
		if (obj->m_state != CS_PLAYING) continue;
		if (player->can_see(obj->m_x, obj->m_y)) new_view.insert(object_id);
	}

	player->send_move_object(player_id);

	for (int object_id : new_view) {
		std::shared_ptr<SESSION> obj = get_session(object_id);
		if (nullptr == obj) continue;

		if (old_view.count(object_id) == 0) {
			player->send_add_object(object_id);
			if (is_pc(object_id)) obj->send_add_object(player_id);
			else obj->wake_up();
		}
		else if (is_pc(object_id)) obj->send_move_object(player_id);
	}

	for (int object_id : old_view) {
		if (new_view.count(object_id) != 0) continue;

		player->send_remove_object(object_id);
		if (is_pc(object_id)) {
			std::shared_ptr<SESSION> other = get_session(object_id);
			if (nullptr != other) other->send_remove_object(player_id);
		}
	}
}

void send_login_fail(SOCKET client, const char* message)
{
	S2C_LoginResult packet;
	packet.size = sizeof(packet);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strcpy_s(packet.message, message);
	WSABUF wsa_buf;
	wsa_buf.buf = reinterpret_cast<char*>(&packet);
	wsa_buf.len = packet.size;
	WSASend(client, &wsa_buf, 1, 0, 0, nullptr, nullptr);
}

int get_new_player_id()
{
	for (;;) {
		int id = player_index++;
		if (id >= NPC_ID_START) return -1;

		auto iter = clients.find(id);
		if (iter == clients.end()) return id;

		std::shared_ptr<SESSION> old = iter->second.load();
		if (nullptr == old || old->m_state == CS_FREE || old->m_state == CS_LOGOUT) return id;
	}
}

void process_npc_move(int npc_id)
{
	std::shared_ptr<SESSION> npc = get_session(npc_id);
	if (nullptr == npc || npc->m_id < NPC_ID_START || npc->m_state != CS_PLAYING) return;

	npc->do_random_move();

	bool has_nearby_player = false;
	for (int object_id : sector_manager.get_objects_in_adjacent_sectors(npc->m_x, npc->m_y)) {
		if (!is_pc(object_id)) continue;
		std::shared_ptr<SESSION> player = get_session(object_id);
		if (nullptr != player && player->can_send() && player->can_see(npc->m_x, npc->m_y)) {
			has_nearby_player = true;
			break;
		}
	}

	if (has_nearby_player) {
		event_type ev;
		ev.obj_id = npc_id;
		ev.event_id = EVENT_NPC_MOVE;
		ev.wakeup_time = std::chrono::system_clock::now() + std::chrono::milliseconds(MOVE_COOL_TIME);
		timer_queue.push(ev);
	}
	else npc->m_active_npc = false;
}

void disconnect(int key)
{
	std::shared_ptr<SESSION> cl = get_session(key);
	if (nullptr == cl || cl->m_id >= NPC_ID_START) return;

	cl->m_state = CS_LOGOUT;
	sector_manager.remove_object_from_sector(key, cl->m_x, cl->m_y);

	std::unordered_set<int> visible;
	{
		std::lock_guard<std::mutex> lock(cl->m_visible_mutex);
		visible = cl->m_visible_objects;
		cl->m_visible_objects.clear();
	}

	for (int object_id : visible) {
		if (!is_pc(object_id)) continue;
		std::shared_ptr<SESSION> other = get_session(object_id);
		if (nullptr != other) other->send_remove_object(key);
	}

	if (cl->m_client != INVALID_SOCKET) {
		closesocket(cl->m_client);
		cl->m_client = INVALID_SOCKET;
	}
	clients[key].store(nullptr);
}

std::shared_ptr<SESSION> get_session(int id)
{
	auto iter = clients.find(id);
	if (iter == clients.end()) return nullptr;
	return iter->second.load();
}
