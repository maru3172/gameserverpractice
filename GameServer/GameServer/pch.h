#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <array>
#include <MSWSock.h>
#include "protocol.h"

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")

constexpr int BUF_SIZE = 200;

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT };

void error_display(const wchar_t* msg, int err_no);

class SESSION;
std::array<SESSION, MAX_PLAYERS> clients;
