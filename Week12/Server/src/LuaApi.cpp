#include "LuaApi.h"
#include "World.h"
#include <lua.hpp>

// C 함수 안에서는 스택에 인자만 있으므로 1..n 인덱스로 읽고 pop 하지 않는다.
namespace LuaApi {

	int API_get_x(lua_State* L)
	{
		int obj_id = static_cast<int>(luaL_checkinteger(L, 1));
		auto obj = World::instance().get_object(obj_id);
		lua_pushinteger(L, obj ? obj->m_x : 0);
		return 1;
	}

	int API_get_y(lua_State* L)
	{
		int obj_id = static_cast<int>(luaL_checkinteger(L, 1));
		auto obj = World::instance().get_object(obj_id);
		lua_pushinteger(L, obj ? obj->m_y : 0);
		return 1;
	}

	int API_SendMessage(lua_State* L)
	{
		int my_id = static_cast<int>(luaL_checkinteger(L, 1));
		int user_id = static_cast<int>(luaL_checkinteger(L, 2));
		const char* mess = luaL_checkstring(L, 3);

		if (auto pl = World::instance().get_player(user_id))
			pl->send_chat(my_id, mess);
		return 0;
	}

	void register_functions(lua_State* L)
	{
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_SendMessage", API_SendMessage);
	}
}
