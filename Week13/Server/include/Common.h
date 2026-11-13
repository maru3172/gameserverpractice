#pragma once

#include <WS2tcpip.h>
#include <MSWSock.h>
#include "PacketDefs.h"
#include "protocol.pb.h"

constexpr int VIEW_RANGE = 7;
constexpr int MAX_BUFFER = 1024;

constexpr int SECTOR_SIZE = VIEW_RANGE * 2 + 1;
constexpr int MAX_SECTORS_X = (WORLD_WIDTH + SECTOR_SIZE - 1) / SECTOR_SIZE;
constexpr int MAX_SECTORS_Y = (WORLD_HEIGHT + SECTOR_SIZE - 1) / SECTOR_SIZE;

constexpr int SECTOR_X(int x) { return x / SECTOR_SIZE; }
constexpr int SECTOR_Y(int y) { return y / SECTOR_SIZE; }
constexpr int SECTOR_ID(int sx, int sy) { return sy * MAX_SECTORS_X + sx; }

enum EVENT_TYPE { EV_RECV, EV_SEND };
enum SESSION_STATE { ST_FREE, ST_CONNECTED, ST_INGAME };

// Overlapped I/O 확장 구조체 (over 가 첫 멤버)
struct OVER_EX {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	char net_buf[MAX_BUFFER];
	EVENT_TYPE event_type;

	explicit OVER_EX(EVENT_TYPE type) : event_type(type)
	{
		ZeroMemory(&over, sizeof(over));
		wsabuf.buf = net_buf;
		wsabuf.len = MAX_BUFFER;
	}
};

void error_display(const char* msg, int err_no);
