#pragma once

#include <WS2tcpip.h>
#include <MSWSock.h>
#include <chrono>
#include <cstring>
#include "Protocol.h"

constexpr int BUF_SIZE = 200;
constexpr int VIEW_RANGE = 5;
constexpr int MOVE_COOL_TIME = 1000;

constexpr int SECTOR_SIZE = VIEW_RANGE * 2 + 1;
constexpr int MAX_SECTORS_X = (WORLD_WIDTH + SECTOR_SIZE - 1) / SECTOR_SIZE;
constexpr int MAX_SECTORS_Y = (WORLD_HEIGHT + SECTOR_SIZE - 1) / SECTOR_SIZE;

constexpr int SECTOR_X(int x) { return x / SECTOR_SIZE; }
constexpr int SECTOR_Y(int y) { return y / SECTOR_SIZE; }
constexpr int SECTOR_ID(int sx, int sy) { return sy * MAX_SECTORS_X + sx; }

inline bool is_pc(int id) { return id < NPC_ID_START; }
inline bool is_npc(int id) { return id >= NPC_ID_START; }

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT, IO_NPC_MOVE, IO_DB_LOGIN };
enum CL_STATE { CS_FREE, CS_CONNECT, CS_DB_WAIT, CS_PLAYING, CS_LOGOUT };

struct DB_RESULT {
	bool success;
	int user_id;
	short x, y;
	bool has_position;
};

// Overlapped I/O 확장 구조체. m_over 가 반드시 첫 멤버여야 한다.
class EXP_OVER {
public:
	WSAOVERLAPPED m_over;
	IOType m_iotype;
	WSABUF m_wsa;
	SOCKET m_client_socket;
	char m_buff[BUF_SIZE];
	DB_RESULT m_db_result;

	explicit EXP_OVER(IOType iot = IO_RECV)
		: m_iotype(iot), m_client_socket(INVALID_SOCKET),
		m_db_result{ false, -1, 0, 0, false }
	{
		ZeroMemory(&m_over, sizeof(m_over));
		m_wsa.buf = m_buff;
		m_wsa.len = BUF_SIZE;
	}
};

void error_display(const wchar_t* msg, int err_no);
