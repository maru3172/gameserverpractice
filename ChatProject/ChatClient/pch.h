#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

const char* SERVER_IP = "127.0.0.1";
constexpr short SERVER_PORT = 4000;
constexpr int BUFFER_SIZE = 4096;

char g_recv_buffer[BUFFER_SIZE];
char g_send_buffer[BUFFER_SIZE];
WSABUF g_recv_wsa_buf{ BUFFER_SIZE, g_recv_buffer };
WSABUF g_send_wsa_buf{ BUFFER_SIZE, g_send_buffer };
WSAOVERLAPPED g_recv_overlapped{}, g_send_overlapped{};
SOCKET g_s_socket;
int g_my_id = -1;

// 채팅 로그 (SFML 윈도우 출력용)
std::vector<sf::String> g_chat_log;
const int MAX_CHAT_LINES = 20;

// 에러 로그
void error_display(const wchar_t* msg, int err_no)
{
    WCHAR* lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, err_no,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    std::wcout << msg;
    std::wcout << L" === 에러 " << lpMsgBuf << std::endl;
    while (true);
    LocalFree(lpMsgBuf);
}