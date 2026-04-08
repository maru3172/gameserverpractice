#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include <MSWSock.h>

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")
constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;

enum IO_Type { IO_SEND, IO_RECV, IO_ACCEPT };

void error_display(const wchar_t* msg, int err_no);

class SESSION;
extern std::unordered_map<long long, SESSION> clients; // key=id, value=¼¼¼Ç
