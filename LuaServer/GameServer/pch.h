#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <array>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include "protocol.h"
#include <tbb/concurrent_unordered_map.h>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <concurrent_priority_queue.h>
#include "include/lua.hpp"

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")

constexpr int BUF_SIZE = 200;
constexpr int VIEW_RANGE = 5;
constexpr int MOVE_COOL_TIME = 1000; // ms

constexpr int EVENT_MOVE = 1;

struct event_type {
	constexpr bool operator < (const event_type& _Left) const
	{
		return (wakeup_time > _Left.wakeup_time);
	}

	int obj_id;
	std::chrono::system_clock::time_point wakeup_time;
	int event_id;
	int target_id;
};

extern concurrency::concurrent_priority_queue<event_type> timer_queue;

extern std::atomic<int> g_num_players;
extern std::atomic<int> player_index;

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT, IO_NPCMOVE, IO_PLAYER_MOVE };
enum CL_STATE { CS_CONNECT, CS_PLAYING, CS_LOGOUT };

void error_display(const wchar_t* msg, int err_no);

class SESSION;
extern tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;

extern HANDLE g_iocp;
extern SOCKET g_server;
