#include "SectorManager.h"

void Sector::add_object(int object_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_objects.insert(object_id);
}

void Sector::remove_object(int object_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_objects.erase(object_id);
}

void Sector::append_objects(std::vector<int>& out)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	out.insert(out.end(), m_objects.begin(), m_objects.end());
}

SectorManager::SectorManager()
{
	for (auto& sector : m_sectors)
		sector = std::make_unique<Sector>();
}

int SectorManager::sector_id(short x, short y)
{
	return SECTOR_ID(SECTOR_X(x), SECTOR_Y(y));
}

void SectorManager::add_object(int object_id, short x, short y)
{
	m_sectors[sector_id(x, y)]->add_object(object_id);
}

void SectorManager::remove_object(int object_id, short x, short y)
{
	m_sectors[sector_id(x, y)]->remove_object(object_id);
}

void SectorManager::update_object(int object_id, short old_x, short old_y, short new_x, short new_y)
{
	int old_sid = sector_id(old_x, old_y);
	int new_sid = sector_id(new_x, new_y);
	if (old_sid == new_sid) return;

	m_sectors[old_sid]->remove_object(object_id);
	m_sectors[new_sid]->add_object(object_id);
}

std::vector<int> SectorManager::get_objects_in_adjacent_sectors(short x, short y)
{
	std::vector<int> result;
	int sx = SECTOR_X(x);
	int sy = SECTOR_Y(y);
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			int nsx = sx + dx;
			int nsy = sy + dy;
			if (nsx < 0 || nsx >= MAX_SECTORS_X || nsy < 0 || nsy >= MAX_SECTORS_Y)
				continue;
			m_sectors[SECTOR_ID(nsx, nsy)]->append_objects(result);
		}
	}
	return result;
}
