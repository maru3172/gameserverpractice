#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <array>
#include <memory>
#include <atomic>
#include <mutex>
#include "protocol.h"
#include <tbb/concurrent_unordered_map.h>
#include <unordered_set>

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")

constexpr int BUF_SIZE = 200;
constexpr int VIEW_RANGE = 5;

constexpr int SECTOR_SIZE = VIEW_RANGE * 2 + 1;
constexpr int MAX_SECTORS_X = (WORLD_WIDTH + SECTOR_SIZE - 1) / SECTOR_SIZE;
constexpr int MAX_SECTORS_Y = (WORLD_HEIGHT + SECTOR_SIZE - 1) / SECTOR_SIZE;

constexpr int SECTOR_X(int x) { return x / SECTOR_SIZE; }
constexpr int SECTOR_Y(int y) { return y / SECTOR_SIZE; }
constexpr int SECTOR_ID(int sx, int sy) { return sy * MAX_SECTORS_X + sx; }

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT };
enum CL_STATE { CS_CONNECT, CS_PLAYING, CS_LOGOUT };

void error_display(const wchar_t* msg, int err_no);

class SESSION;
extern tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;
extern std::atomic<int> player_index;

extern HANDLE g_iocp;
extern SOCKET g_server;
