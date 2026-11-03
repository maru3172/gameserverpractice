#include "pch.h"
#include "SESSION.h"

std::atomic<int> g_num_players = 0;


SESSION::SESSION()
{
	std::cout << "SESSION Creation Error!\n";
	exit(-1);
}

SESSION::SESSION(SOCKET s, int id) : m_client(s), m_id(id)
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
	if (L != nullptr) lua_close(L);
	if (is_player()) g_num_players--;
}

bool SESSION::is_npc() const
{
	return m_id >= NPC_ID_START;
}

bool SESSION::is_player() const
{
	return m_id < NPC_ID_START;
}

bool SESSION::is_visible(short x, short y)
{
	return abs(m_x - x) <= VIEW_RANGE && abs(m_y - y) <= VIEW_RANGE;
}

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&m_recv_over.m_over, 0, sizeof(m_recv_over.m_over));
	WSARecv(m_client, &m_recv_over.m_wsa, 1, 0, &recv_flag, &m_recv_over.m_over, nullptr);
}

void SESSION::do_send(int num_bytes, char* mess)
{
	EXP_OVER* o = new EXP_OVER(IO_SEND);
	o->m_wsa.len = num_bytes;
	memcpy(o->m_buff, mess, num_bytes);
	WSASend(m_client, &o->m_wsa, 1, 0, 0, &o->m_over, nullptr);
}

void SESSION::send_avatar_info()
{
	// '나' 자신의 정보를 클라이언트에게 전송
	S2C_AvatarInfo packet;
	packet.size = sizeof(S2C_AvatarInfo);
	packet.type = S2C_AVATAR_INFO;
	packet.playerId = m_id;
	packet.x = m_x;
	packet.y = m_y;
	do_send(packet.size, reinterpret_cast<char*>(&packet)); // 클라이언트에게 전송
}

void SESSION::send_move_packet(int mover)
{
	// 해당 플레이어의 이동을 클라이언트에게 전송
	S2C_MoveObject packet;
	packet.size = sizeof(S2C_MoveObject);
	packet.type = S2C_MOVE_OBJECT;
	packet.object_id = mover;
	std::shared_ptr<SESSION> pl = clients[mover];
	if (nullptr == pl) return;
	packet.x = pl->m_x;
	packet.y = pl->m_y;
	packet.move_time = pl->m_move_time;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_add_player(int player_id)
{
	// 다른 플레이어의 접속을 클라이언트에게 알림
	S2C_AddObject packet;
	packet.size = sizeof(S2C_AddObject);
	packet.type = S2C_ADD_OBJECT;
	packet.object_id = player_id;
	std::shared_ptr<SESSION> pl = clients[player_id].load();
	if (nullptr == pl) return;
	memcpy(packet.obj_name, pl->m_username, sizeof(packet.obj_name));
	packet.x = pl->m_x;
	packet.y = pl->m_y;

	m_visible_mutex.lock();
	if (m_visible_players.count(player_id) > 0) {
		m_visible_mutex.unlock();
		return;
	}
	m_visible_players.insert(player_id);
	m_visible_mutex.unlock();

	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_login_success()
{
	// 로그인 성공했음을 플레이어에게 알림
	S2C_LoginResult packet;
	packet.size = sizeof(S2C_LoginResult);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = true;
	strncpy_s(packet.message, "Login successful.", sizeof(packet.message));
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_remove_player(int player_id)
{
	// 플레이어 접속 종료
	S2C_RemoveObject packet;
	packet.size = sizeof(S2C_RemoveObject);
	packet.type = S2C_REMOVE_OBJECT;
	packet.object_id = player_id;

	m_visible_mutex.lock();
	if (m_visible_players.count(player_id) == 0) {
		m_visible_mutex.unlock();
		return;
	}
	m_visible_players.erase(player_id);
	m_visible_mutex.unlock();

	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_chat(int player_id, char* mess)
{
	S2C_ChatMessage packet;;
	packet.size = sizeof(S2C_ChatMessage);
	packet.type = S2C_CHAT_MESSAGE;
	packet.object_id = player_id;

	strcpy_s(packet.message, mess);
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
		// 로그인 패킷 받아오고
		C2S_Login* packet = reinterpret_cast<C2S_Login*>(p);
		strncpy_s(m_username, packet->username, MAX_NAME_LEN);
		// 접속했다고 로그
		std::cout << ".";
		send_avatar_info();
		m_state = CS_PLAYING; // 자신에게 본인이 접속했다 알림

		// 브로드캐스트
		for (auto& other : clients) {
			std::shared_ptr<SESSION> pl = other.second.load();
			if (nullptr == pl) continue;
			if (pl->m_id == m_id) continue;
			if (false == is_visible(pl->m_x, pl->m_y)) continue;
			if (pl->m_state != CS_PLAYING) continue;
			send_add_player(pl->m_id);
			pl->send_add_player(m_id);
		}
	}
	break;
	case C2S_MOVE:
	{
		// 이동 패킷 받아오고
		C2S_Move* packet = reinterpret_cast<C2S_Move*>(p);
		m_move_time = packet->move_time;
		auto new_x = packet->x;
		auto new_y = packet->y;

		auto old_v = m_visible_players;

		if (abs(m_x - new_x) > 1 || abs(m_y - new_y) > 1) {
			std::cout << "Invalid move packet received from player[" << m_id << "].\n";
			send_move_packet(m_id);  // 클라이언트의 위치를 서버의 위치로 보정
			return false;
		}

		m_x = new_x;
		m_y = new_y;

		std::unordered_set<int> new_v;
		for (auto& c : clients) {
			std::shared_ptr<SESSION> pl = c.second.load();
			if (nullptr == pl) continue;
			if (pl->m_id == m_id) continue;
			if (pl->m_state != CS_PLAYING) continue;
			if (is_visible(pl->m_x, pl->m_y))
			{
				if (pl->is_npc()) pl->move_notice(m_id);
				new_v.insert(pl->m_id);
			}
		}

		//  STRESS TEST이므로 이동 delay를 전해 주어야 한다.
		if (packet->move_time != 0) send_move_packet(m_id);

		for (int id : new_v) {
			if (old_v.count(id) == 0) {  // 새로 시야에 들어옴
				send_add_player(id);
				std::shared_ptr<SESSION> pl = clients[id].load();
				if (nullptr == pl) continue;
				if (true == pl->is_player()) pl->send_add_player(m_id);
			}
			else {  // 여전히 시야에 있음
				std::shared_ptr<SESSION> pl = clients[id].load();
				if (nullptr == pl) continue;
				if (true == pl->is_player()) pl->send_move_packet(m_id);
			}
		}
		for (int id : old_v) {
			if (new_v.count(id) == 0) {  // 시야에서 사라짐
				send_remove_player(id);
				std::shared_ptr<SESSION> pl = clients[id].load();
				if (nullptr == pl) continue;
				if (true == pl->is_player()) pl->send_remove_player(m_id);
			}
		}
	}
	break;
	default: // 플레이어로부터 알 수 없는 패킷이 전송됨
		std::cout << "Unknown packet type received from player[" << m_id << "].\n";
		return false;
		break;
	}
	return true;
}

void SESSION::do_random_move()
{
	if (false == is_npc()) return;

	npc_last_move_time = std::chrono::system_clock::now();
	int dir = rand() % 4;
	short new_x = m_x, new_y = m_y;
	switch (dir) {
	case 0: new_y -= 1; break; // Up
	case 1: new_y += 1; break; // Down
	case 2: new_x -= 1; break; // Left
	case 3: new_x += 1; break; // Right
	}

	// 월드 경계 체크
	if (new_x < 0 || new_x >= WORLD_WIDTH || new_y < 0 || new_y >= WORLD_HEIGHT) {
		return;
	}

	std::unordered_set<int> old_v;
	for (auto& obj : clients) {
		std::shared_ptr<SESSION> pl = obj.second.load();
		if (nullptr == pl) continue;
		if (pl->is_player() == false) continue;
		if (pl->m_state != CS_PLAYING) continue;
		if (is_visible(pl->m_x, pl->m_y))
			old_v.insert(pl->m_id);
	}

	m_x = new_x;
	m_y = new_y;
	std::unordered_set<int> new_v;
	for (auto& obj : clients) {
		std::shared_ptr<SESSION> pl = obj.second.load();
		if (nullptr == pl) continue;
		if (pl->is_player() == false) continue;
		if (pl->m_state != CS_PLAYING) continue;
		if (is_visible(pl->m_x, pl->m_y))
			new_v.insert(pl->m_id);
	}

	for (int pl : new_v) {
		if (old_v.count(pl) == 0) {  // 새로 시야에 들어옴
			std::shared_ptr<SESSION> pl_session = clients[pl].load();
			if (nullptr == pl_session) continue;
			pl_session->send_add_player(m_id);
		}
		else {  // 여전히 시야에 있음
			std::shared_ptr<SESSION> pl_session = clients[pl].load();
			if (nullptr == pl_session) continue;
			pl_session->send_move_packet(m_id);
		}
	}
	for (int pl : old_v) {
		if (new_v.count(pl) == 0) {  // 시야에서 사라짐
			std::shared_ptr<SESSION> pl_session = clients[pl].load();
			if (nullptr == pl_session) continue;
			pl_session->send_remove_player(m_id);
		}
	}
}

void SESSION::move_notice(int mover)
{
	EXP_OVER* o = new EXP_OVER(IO_PLAYER_MOVE);
	o->m_target_id = mover;
	PostQueuedCompletionStatus(g_iocp, 0, m_id, &o->m_over);
}

void send_login_fail(SOCKET client, const char* message)
{
	S2C_LoginResult packet;
	packet.size = sizeof(S2C_LoginResult);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strncpy_s(packet.message, message, sizeof(packet.message));
	WSABUF wsa_buf;
	wsa_buf.buf = reinterpret_cast<char*>(&packet);
	wsa_buf.len = packet.size;
	WSASend(client, &wsa_buf, 1, 0, 0, nullptr, nullptr);
}

void disconnect(int key)
{
	std::cout << "client[" << key << "] Disconnected.\n";
	std::shared_ptr<SESSION> cl = clients[key].load();
	if (nullptr != cl) {
		cl->m_state = CS_LOGOUT;
		auto visible_copy = cl->m_visible_players;
		for (auto& other : visible_copy) {
			std::shared_ptr<SESSION> o = clients[other];
			if (nullptr == o) continue;
			if (CS_PLAYING == o->m_state)
				o->send_remove_player(key);
		}
		closesocket(cl->m_client);
		cl->m_client = INVALID_SOCKET;
	}
	clients[key].store(nullptr);
}

int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);
	

	std::shared_ptr<SESSION> cl = clients[user_id];
	if (nullptr == cl)
	{
		std::cout << "API_SendMessage: User not found.\n";
		return 0;
	}
	else cl->send_chat(my_id, mess);
	return 0;
}

int API_get_y(lua_State* L)
{
	int user_id = (int)lua_tointeger(L, -1);
	
	std::shared_ptr<SESSION> cl = clients[user_id];
	if (nullptr != cl)
	{
		int y = cl->m_y;
		lua_pushnumber(L, y);
		return 1;
	}
	else lua_pushnumber(L, -1);
	return 1;
}

int API_get_x(lua_State* L)
{
	int user_id = (int)lua_tointeger(L, -1);
	
	std::shared_ptr<SESSION> cl = clients[user_id];
	if (nullptr != cl)
	{
		int x = cl->m_x;
		lua_pushnumber(L, x);
		return 1;
	}
	else lua_pushnumber(L, -1);
	return 1;
}

void initialize_npcs()
{
	std::cout << "NPC Initialization begins.\n";
	for (int i = 0; i < NUM_NPCS; ++i) {
		int npc_id = i + NPC_ID_START;
		auto npc = std::make_shared<SESSION>(INVALID_SOCKET, npc_id);
		npc->npc_last_move_time = std::chrono::system_clock::now();
		npc->m_state = CS_PLAYING;
		sprintf_s(npc->m_username, "NPC_%d", npc_id);
		npc->m_x = rand() % WORLD_WIDTH;
		npc->m_y = rand() % WORLD_HEIGHT;
		event_type ev;
		ev.event_id = EVENT_MOVE;
		ev.obj_id = npc_id;
		ev.wakeup_time = npc->npc_last_move_time + std::chrono::seconds(1);
		timer_queue.push(ev);
		auto L = luaL_newstate();
		npc->L = L;

		luaL_openlibs(L);
		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_pushinteger(L, npc_id);
		lua_setglobal(L, "NPC_ID");

		luaL_loadfile(L, "NPC.lua");
		lua_pcall(L, 0, 0, 0);

		clients[npc_id] = npc;
	}
	std::cout << "NPC Initialization ended.\n";
}
