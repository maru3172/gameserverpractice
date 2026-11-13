#pragma once

struct lua_State;

// npc.lua 에서 호출하는 C++ API
namespace LuaApi {
	void register_functions(lua_State* L);

	int API_get_x(lua_State* L);          // API_get_x(obj_id) -> x
	int API_get_y(lua_State* L);          // API_get_y(obj_id) -> y
	int API_SendMessage(lua_State* L);    // API_SendMessage(npc_id, player_id, "message")
}
