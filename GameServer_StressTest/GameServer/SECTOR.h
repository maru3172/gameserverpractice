#pragma once

// 섹터 내 플레이어 관리
class SECTOR
{
public:
	SECTOR();

	~SECTOR();

	void add_player(int player_id);
	void remove_player(int player_id);

	std::mutex m_mutex;
	std::unordered_set<int> m_players;
};

