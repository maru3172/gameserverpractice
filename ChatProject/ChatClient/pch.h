#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr short SERVER_PORT = 4000;
constexpr int BUFFER_SIZE = 4096;

extern char g_recv_buffer[];
extern char g_send_buffer[];
extern WSABUF g_recv_wsa_buf;
extern WSABUF g_send_wsa_buf;
extern WSAOVERLAPPED g_recv_overlapped, g_send_overlapped;
extern SOCKET g_s_socket;
extern int g_my_id;

// 채팅 로그 (SFML 윈도우 출력용)
extern std::vector<sf::String> g_chat_log;
constexpr int MAX_CHAT_LINES = 20;

// 채팅 로그 띄우고 저장
void add_chat_line(const sf::String& line);

// 에러 로그
void error_display(const wchar_t* msg, int err_no);
