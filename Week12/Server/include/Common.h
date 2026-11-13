#pragma once

#include <sdkddkver.h>
#include <asio.hpp>
#include <chrono>
#include "Protocol.h"

constexpr int VIEW_RANGE = 5;
constexpr int NET_BUFF_SIZE = 8192;
constexpr auto NPC_MOVE_INTERVAL = std::chrono::seconds(1);

constexpr int SECTOR_SIZE = VIEW_RANGE * 2 + 1;
constexpr int MAX_SECTORS_X = (W_WIDTH + SECTOR_SIZE - 1) / SECTOR_SIZE;
constexpr int MAX_SECTORS_Y = (W_HEIGHT + SECTOR_SIZE - 1) / SECTOR_SIZE;

constexpr int SECTOR_X(int x) { return x / SECTOR_SIZE; }
constexpr int SECTOR_Y(int y) { return y / SECTOR_SIZE; }
constexpr int SECTOR_ID(int sx, int sy) { return sy * MAX_SECTORS_X + sx; }

inline bool is_pc(int object_id) { return object_id < MAX_USER; }
inline bool is_npc(int object_id) { return !is_pc(object_id); }

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
