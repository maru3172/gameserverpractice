#include "pch.h"
#include "SESSION.h"

tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;

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
	closesocket(m_client);
}

bool SESSION::is_visible(short other_x, short other_y) // 3D게임에선 원, 2D는 사각형, 내 시야 거리 안에 존재하는가?
{
	return abs(m_x - other_x) <= VIEW_RANGE && abs(m_y - other_y) <= VIEW_RANGE;
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

void SESSION::do_move(DIRECTION dir)
{
	auto old_v = m_visible_players; // 보였던 플레이어 기록, 새로이 받아올 필요 없이 캐시 적중률 상승

	switch (dir)
	{
	case UP: m_y = max(0, m_y - 1); break;
	case DOWN: m_y = min(WORLD_HEIGHT - 1, m_y + 1); break;
	case LEFT: m_x = max(0, m_x - 1); break;
	case RIGHT: m_x = min(WORLD_WIDTH - 1, m_x + 1); break;
	}

	std::unordered_set<int> new_v;
	for (auto& cl : clients) {
		std::shared_ptr<SESSION> pl = cl.second.load();
		if (nullptr == pl) continue;
		if (CS_PLAYING != pl->m_state) continue;
		if (pl->m_id == m_id) continue;
		if (is_visible(pl->m_x, pl->m_y)) new_v.insert(pl->m_id);
	}

	send_move_packet(m_id); // 자기 자신의 이동을 전달함

	for (int id : new_v) {
		if (old_v.count(id) == 0) {
			// new_v에 있는데 old_v에 없음 → 새로 시야에 들어온 플레이어
			send_add_player(id); // 나한테 저 사람 추가
			std::shared_ptr<SESSION> pl = clients[id].load();
			if (nullptr == pl) continue;
			pl->send_add_player(m_id); // 저 사람한테 나 추가
		}
	}

	for (int id : old_v) {
		if (new_v.count(id) == 0) {
			// old_v에 있는데 new_v에 없음 → 시야에서 벗어난 플레이어
			send_remove_player(id); // 나한테서 저 사람 제거
			std::shared_ptr<SESSION> pl = clients[id].load();
			if (nullptr == pl) continue;
			pl->send_remove_player(m_id); // 저 사람한테서 나 제거
		}
		else
		{
			// old_v에도 있고 new_v에도 있음 → 계속 시야 안에 있던 플레이어
			std::shared_ptr<SESSION> pl = clients[id].load();
			if (nullptr == pl) continue;
			pl->send_move_packet(m_id); // 이동 정보만 전송
		}
	}
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
	S2C_MovePlayer packet;
	packet.size = sizeof(S2C_MovePlayer);
	packet.type = S2C_MOVE_PLAYER;
	packet.playerId = mover;
	std::shared_ptr<SESSION> pl = clients[mover].load();
	if (nullptr == pl) return;
	packet.x = pl->m_x;
	packet.y = pl->m_y;
	packet.move_time = pl->m_move_time;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_add_player(int player_id)
{
	// 다른 플레이어의 접속을 클라이언트에게 알림
	S2C_AddPlayer packet;
	packet.size = sizeof(S2C_AddPlayer);
	packet.type = S2C_ADD_PLAYER;
	packet.playerId = player_id;
	std::shared_ptr<SESSION> pl = clients[player_id].load();
	if (nullptr == pl) return;
	memcpy(packet.username, pl->m_username, sizeof(packet.username));  // 세션 정보 가져온 후, 보낼 플레이어 정보를 패킷에 복사
	packet.x = pl->m_x;
	packet.y = pl->m_y;

	m_visible_mutex.lock(); // 여러곳에서 불려짐, lock 필요
	if (m_visible_players.count(player_id) > 0) {
		m_visible_mutex.unlock();
		return; // 이미 그 플레이어가 시야에 존재, 추가 X
	}
	m_visible_players.insert(player_id); // 시야에 존재한적 X, 추가 필요
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
	S2C_RemovePlayer packet;
	packet.size = sizeof(S2C_RemovePlayer);
	packet.type = S2C_REMOVE_PLAYER;
	packet.playerId = player_id;

	m_visible_mutex.lock(); // 여러곳에서 불려짐, lock 필요
	if (m_visible_players.count(player_id) == 0) {
		m_visible_mutex.unlock();
		return; // 시야 목록에 없던 플레이어이므로 제거할 이유도 없음
	}
	m_visible_players.erase(player_id); // 내 시야에 있던 플레이어 제거
	m_visible_mutex.unlock();

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
		std::cout << "Player[" << m_id << "] logged in as " << m_username << std::endl;
		m_state = CS_PLAYING; // 자신에게 본인이 접속했다 알림
		send_avatar_info();

		// 브로드캐스트
		for (auto& other : clients) {
			std::shared_ptr<SESSION> pl = other.second.load();
			if (nullptr == pl) continue;
			if (pl->m_id == m_id) continue;
			if (!is_visible(pl->m_x, pl->m_y)) continue;
			if (pl->m_state != CS_PLAYING) continue;
			send_add_player(pl->m_id);
			pl->send_add_player(m_id);
		}
	}
	break;
	case C2S_MOVE:
	{
		C2S_Move* packet = reinterpret_cast<C2S_Move*>(p);
		DIRECTION dir = packet->dir;
		m_move_time = packet->move_time;
		do_move(dir);
	}
	break;
	default: // 플레이어로부터 알 수 없는 패킷이 전송됨
		std::cout << "Unknown packet type received from player[" << m_id << "].\n";
		return false;
		break;
	}
	return true;
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
	if (nullptr != cl)
	{
		// 자신의 연결이 끊겼을 때
		cl->m_state = CS_LOGOUT;
		// 자신의 시야 목록에 있던 애들한테만 remove 전송
		for (auto& other : cl->m_visible_players) {
			std::shared_ptr<SESSION> o = clients[other].load();
			if (nullptr == o) continue;
			if (CS_PLAYING == o->m_state) o->send_remove_player(key);
		}
		closesocket(cl->m_client);
		cl->m_client = INVALID_SOCKET;
	}
	clients[key].store(nullptr);
}
