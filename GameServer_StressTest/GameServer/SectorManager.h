#pragma once

class SECTOR;

// 섹터 관리, 플레이어 추가 및 삭제
class SectorManager
{
public:
	SectorManager();

	void add_player_to_sector(int player_id, short x, short y); // 섹터 플레이어 추가
	void remove_player_from_sector(int player_id, short x, short y); // 섹터 플레이어 삭제

	std::vector<int> get_players_in_sector(short x, short y); // 해당 플레이어가 속한 섹터의 플레이어 정보들
	std::vector<int> get_players_in_adjacent_sectors(short x, short y); // 해당 플레이어가 속한 섹터와 인접한 8개의 섹터들의 플레이어 정보들

private:
	std::array<std::shared_ptr<SECTOR>, MAX_SECTORS_X * MAX_SECTORS_Y> sectors;
};

