#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <Windows.h>
#include <chrono>

#include "..\GameServer\Protocol.h"

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;

extern int g_left_x;
extern int g_top_y;
extern int g_myid;

extern sf::TcpSocket s_socket;
extern sf::RenderWindow* g_window;
extern sf::Font g_font;

class OBJECT;
extern std::unordered_map <int, OBJECT> players;

extern sf::Texture* board, * pieces;
