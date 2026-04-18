#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>

#include "..\GameServer\Protocol.h"

constexpr auto SCREEN_WIDTH = WORLD_WIDTH;
constexpr auto SCREEN_HEIGHT = WORLD_HEIGHT;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;
constexpr auto MAX_USER = MAX_PLAYERS;
constexpr int BUF_SIZE = 1024;

extern int g_left_x;
extern int g_top_y;
extern int g_myid;

extern sf::TcpSocket socket;
extern sf::RenderWindow* g_window;
extern sf::Font* g_font;

extern std::string avatar_name;

extern sf::Texture* board, * pieces;
