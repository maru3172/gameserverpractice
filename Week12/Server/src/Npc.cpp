#include "Npc.h"
#include "World.h"
#include "LuaApi.h"
#include <iostream>
#include <lua.hpp>

Npc::Npc(int id) : GameObject(id)
{
	m_x = static_cast<short>(rand() % W_WIDTH);
	m_y = static_cast<short>(rand() % W_HEIGHT);
	sprintf_s(m_name, "NPC%d", id);
	m_state = ST_INGAME;
}

Npc::~Npc()
{
	if (m_lua) lua_close(m_lua);
}

// m_lua_lock 을 잡은 상태에서 호출해야 한다.
lua_State* Npc::lua()
{
	if (m_lua) return m_lua;

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	LuaApi::register_functions(L);

	if (luaL_loadfile(L, "npc.lua") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
		std::cout << "[Lua] npc.lua load error: " << lua_tostring(L, -1) << "\n";
		lua_pop(L, 1);
	}
	else {
		lua_getglobal(L, "set_uid");
		lua_pushinteger(L, m_id);
		if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
			std::cout << "[Lua] set_uid error: " << lua_tostring(L, -1) << "\n";
			lua_pop(L, 1);
		}
	}
	m_lua = L;
	return m_lua;
}

void Npc::call_player_move_event(int player_id)
{
	std::lock_guard<std::mutex> lock(m_lua_lock);
	lua_State* L = lua();
	lua_getglobal(L, "event_player_move");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_pushinteger(L, player_id);
	if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
		std::cout << "[Lua] event_player_move error: " << lua_tostring(L, -1) << "\n";
		lua_pop(L, 1);
	}
}

void Npc::wake_up(int waker_id)
{
	auto& world = World::instance();
	int npc_id = m_id;

	asio::post(world.io(), [npc_id, waker_id]() {
		if (Npc* npc = World::instance().get_npc(npc_id))
			npc->call_player_move_event(waker_id);
		});

	bool expected = false;
	if (!m_active.compare_exchange_strong(expected, true)) return;

	asio::post(world.io(), [npc_id]() {
		if (Npc* npc = World::instance().get_npc(npc_id)) {
			npc->random_move();
			World::instance().add_timer(npc_id, std::chrono::duration_cast<std::chrono::milliseconds>(NPC_MOVE_INTERVAL));
		}
		});
}

bool Npc::has_nearby_player()
{
	auto& world = World::instance();
	for (int id : world.sectors().get_objects_in_adjacent_sectors(m_x, m_y)) {
		if (!is_pc(id)) continue;
		auto pl = world.get_player(id);
		if (pl && pl->m_state == ST_INGAME && pl->can_see(*this)) return true;
	}
	return false;
}

void Npc::on_timer()
{
	if (has_nearby_player()) {
		random_move();
		World::instance().add_timer(m_id, std::chrono::duration_cast<std::chrono::milliseconds>(NPC_MOVE_INTERVAL));
	}
	else {
		m_active = false;
	}
}

void Npc::random_move()
{
	auto& world = World::instance();
	short old_x = m_x, old_y = m_y;

	std::unordered_set<int> old_viewers;
	for (int id : world.sectors().get_objects_in_adjacent_sectors(old_x, old_y)) {
		if (!is_pc(id)) continue;
		auto pl = world.get_player(id);
		if (pl && pl->m_state == ST_INGAME && pl->can_see(old_x, old_y))
			old_viewers.insert(id);
	}

	switch (rand() % 4) {
	case 0: if (m_x < W_WIDTH - 1) m_x++; break;
	case 1: if (m_x > 0) m_x--; break;
	case 2: if (m_y < W_HEIGHT - 1) m_y++; break;
	case 3: if (m_y > 0) m_y--; break;
	}
	world.sectors().update_object(m_id, old_x, old_y, m_x, m_y);

	std::unordered_set<int> new_viewers;
	for (int id : world.sectors().get_objects_in_adjacent_sectors(m_x, m_y)) {
		if (!is_pc(id)) continue;
		auto pl = world.get_player(id);
		if (!pl || pl->m_state != ST_INGAME || !pl->can_see(*this)) continue;
		new_viewers.insert(id);
		if (pl->in_view(m_id))
			pl->send_move_object(m_id);
		else
			pl->send_add_object(m_id);
	}

	for (int id : old_viewers) {
		if (new_viewers.count(id)) continue;
		if (auto pl = world.get_player(id))
			pl->send_remove_object(m_id);
	}
}
