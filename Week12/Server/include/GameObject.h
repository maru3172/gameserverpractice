#pragma once

#include <atomic>
#include <mutex>
#include <unordered_set>
#include "Common.h"

// 플레이어와 NPC 의 공통 부모. 위치/이름/상태/시야 목록을 가진다.
class GameObject {
public:
	explicit GameObject(int id) : m_id(id) { m_name[0] = 0; }
	virtual ~GameObject() = default;

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;

	int id() const { return m_id; }
	bool can_see(short x, short y) const
	{
		return abs(m_x - x) <= VIEW_RANGE && abs(m_y - y) <= VIEW_RANGE;
	}
	bool can_see(const GameObject& other) const { return can_see(other.m_x, other.m_y); }

	bool add_view(int id)
	{
		std::lock_guard<std::mutex> lock(m_view_lock);
		return m_view_list.insert(id).second;
	}
	bool remove_view(int id)
	{
		std::lock_guard<std::mutex> lock(m_view_lock);
		return m_view_list.erase(id) != 0;
	}
	bool in_view(int id)
	{
		std::lock_guard<std::mutex> lock(m_view_lock);
		return m_view_list.count(id) != 0;
	}
	std::unordered_set<int> copy_view()
	{
		std::lock_guard<std::mutex> lock(m_view_lock);
		return m_view_list;
	}
	std::unordered_set<int> take_view()
	{
		std::lock_guard<std::mutex> lock(m_view_lock);
		std::unordered_set<int> result;
		result.swap(m_view_list);
		return result;
	}

public:
	std::atomic<S_STATE> m_state{ ST_FREE };
	short m_x = 0, m_y = 0;
	char m_name[NAME_SIZE];
	unsigned m_last_move_time = 0;

protected:
	const int m_id;
	std::unordered_set<int> m_view_list;
	std::mutex m_view_lock;
};
