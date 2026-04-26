#include "pch.h"
#include "SectorManager.h"
#include "SECTOR.h"

SectorManager::SectorManager()
{
	for (int i = 0; i < MAX_SECTORS_X * MAX_SECTORS_Y; ++i) {
		sectors[i] = std::make_shared<SECTOR>();
	}
}

void SectorManager::add_player_to_sector(int player_id, short x, short y)
{
	int sector_id = SECTOR_ID(SECTOR_X(x), SECTOR_Y(y)); // 현재 위치한 좌표의 섹터 ID 확보
	sectors[sector_id]->add_player(player_id);
}

void SectorManager::remove_player_from_sector(int player_id, short x, short y)
{
	int sector_id = SECTOR_ID(SECTOR_X(x), SECTOR_Y(y));
	sectors[sector_id]->remove_player(player_id);
}

std::vector<int> SectorManager::get_players_in_sector(short x, short y)
{
	int sid = SECTOR_ID(SECTOR_X(x), SECTOR_Y(y));
	std::lock_guard<std::mutex> lg(sectors[sid]->m_mutex);
	return { sectors[sid]->m_players.begin(), sectors[sid]->m_players.end() };
}

std::vector<int> SectorManager::get_players_in_adjacent_sectors(short x, short y)
{
	std::vector<int> result;
	int sx = SECTOR_X(x);
	int sy = SECTOR_Y(y);
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			int nsx = sx + dx;
			int nsy = sy + dy;
			if (nsx < 0 || nsx >= MAX_SECTORS_X || nsy < 0 || nsy >= MAX_SECTORS_Y) continue;
			int sid = SECTOR_ID(nsx, nsy);
			std::lock_guard<std::mutex> lg(sectors[sid]->m_mutex);
			result.insert(result.end(), sectors[sid]->m_players.begin(), sectors[sid]->m_players.end());
		}
	}
	return result;
}
