#include "pch.h"
#include "SESSION.h"

std::array<SESSION, MAX_PLAYERS> clients;

SESSION::SESSION()
{
	m_is_connected = false;
	m_id = 999;
	m_client = INVALID_SOCKET;
	m_recv_over.m_iotype = IO_RECV;
	m_x = 0; 		m_y = 0;
	m_prev_recv = 0;
}

SESSION::~SESSION()
{
	if (m_is_connected) closesocket(m_client);
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
	S2C_MovePlayer packet;
	packet.size = sizeof(S2C_MovePlayer);
	packet.type = S2C_MOVE_PLAYER;
	packet.playerId = mover;
	packet.x = clients[mover].m_x;
	packet.y = clients[mover].m_y;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::send_add_player(int player_id)
{
	// 다른 플레이어의 접속을 클라이언트에게 알림
	S2C_AddPlayer packet;
	packet.size = sizeof(S2C_AddPlayer);
	packet.type = S2C_ADD_PLAYER;
	packet.playerId = player_id;
	SESSION& pl = clients[player_id];
	memcpy(packet.username, pl.m_username, sizeof(packet.username));  // 세션 정보 가져온 후, 보낼 플레이어 정보를 패킷에 복사
	packet.x = pl.m_x;
	packet.y = pl.m_y;
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
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void SESSION::process_packet(unsigned char* p)
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

		// 브로드캐스트
		for (auto& other : clients) {
			if (!other.m_is_connected) continue; // 접속해있지 않았다면
			if (other.m_id == m_id) continue; // 본인이라면
			other.send_add_player(m_id);
			clients[m_id].send_add_player(other.m_id);
		}

		send_avatar_info(); // 자신에게 본인이 접속했다 알림
	}
	break;
	case C2S_MOVE:
	{
		// 이동 패킷 받아오고
		C2S_Move* packet = reinterpret_cast<C2S_Move*>(p);
		DIRECTION dir = packet->dir; // 어디로 이동을 입력했는지 받음

		switch (dir)
		{
		case UP: m_y = max(0, m_y - 1); break;
		case DOWN: m_y = min(WORLD_HEIGHT - 1, m_y + 1); break;
		case LEFT: m_x = max(0, m_x - 1); break;
		case RIGHT:  m_x = min(WORLD_WIDTH - 1, m_x + 1); break;
		}
		
		// 이동 로그
		std::cout << "Player[" << m_id << "] moved to (" << m_x << ", " << m_y << ")\n";
		
		// 브로드캐스트
		for (auto& cl : clients)
			if (true == cl.m_is_connected) cl.send_move_packet(m_id);
	}
	break;
	default: // 플레이어로부터 알 수 없는 패킷이 전송됨
		std::cout << "Unknown packet type received from player[" << m_id << "].\n";
		break;
	}
}
