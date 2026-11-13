#pragma once

#include <atomic>
#include <mutex>
#include "GameObject.h"

struct lua_State;

// Lua 스크립트(npc.lua)로 동작하는 NPC.
// Lua VM 은 NPC 가 처음 깨어날 때 생성한다 (20만 개를 미리 만들면 메모리가 수 GB 필요).
class Npc : public GameObject {
public:
	explicit Npc(int id);
	~Npc() override;

	void wake_up(int waker_id);      // 플레이어가 근처에서 움직였을 때
	void on_timer();                 // 주기적 이동 (타이머 스레드에서 호출)

private:
	void call_player_move_event(int player_id);
	void random_move();
	bool has_nearby_player();
	lua_State* lua();

	std::atomic<bool> m_active{ false };
	lua_State* m_lua = nullptr;
	std::mutex m_lua_lock;
};
