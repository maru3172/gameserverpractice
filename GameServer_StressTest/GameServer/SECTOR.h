#pragma once

// 섹터 내 플레이어 관리
class SECTOR
{
public:
	void add_object(int object_id);
	void remove_object(int object_id);

	std::unordered_set<int> m_objects;
	std::mutex m_mutex;
};
