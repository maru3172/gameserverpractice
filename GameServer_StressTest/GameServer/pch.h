#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <array>
#include <MSWSock.h>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include "protocol.h"
#include <tbb/concurrent_unordered_map.h>

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")

constexpr int BUF_SIZE = 200;

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT };
enum CL_STATE { CS_CONNECT, CS_PLAYING, CS_LOGOUT };

void error_display(const wchar_t* msg, int err_no);

class SESSION;
extern tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;
extern std::atomic<int> player_index;

extern HANDLE g_iocp;
extern SOCKET g_server;
