#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <array>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <chrono>
#include <atomic>
#include <memory>
#include <concurrent_priority_queue.h>
#include <tbb/concurrent_unordered_map.h>
#include "Protocol.h"

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")

constexpr int BUF_SIZE = 200;
constexpr int VIEW_RANGE = 5;
constexpr int MOVE_COOL_TIME = 1000;
constexpr int EVENT_NPC_MOVE = 1;

constexpr int SECTOR_SIZE = VIEW_RANGE * 2 + 1;
constexpr int MAX_SECTORS_X = (WORLD_WIDTH + SECTOR_SIZE - 1) / SECTOR_SIZE;
constexpr int MAX_SECTORS_Y = (WORLD_HEIGHT + SECTOR_SIZE - 1) / SECTOR_SIZE;

constexpr int SECTOR_X(int x) { return x / SECTOR_SIZE; }
constexpr int SECTOR_Y(int y) { return y / SECTOR_SIZE; }
constexpr int SECTOR_ID(int sx, int sy) { return sy * MAX_SECTORS_X + sx; }

struct event_type
{
	constexpr bool operator < (const event_type& other) const
	{
		return wakeup_time > other.wakeup_time;
	}

	int obj_id;
	std::chrono::system_clock::time_point wakeup_time;
	int event_id;
};

extern concurrency::concurrent_priority_queue<event_type> timer_queue;

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT, IO_NPC_MOVE };
enum CL_STATE { CS_FREE, CS_CONNECT, CS_PLAYING, CS_LOGOUT };

void error_display(const wchar_t* msg, int err_no);

class SESSION;
extern tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;
extern std::atomic<int> player_index;

class SectorManager;
extern SectorManager sector_manager;

extern HANDLE g_iocp;
extern SOCKET g_server;

bool is_pc(int id);

bool is_npc(int id);