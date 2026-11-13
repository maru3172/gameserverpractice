#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>
#include "Common.h"

class Sector {
public:
	void add_object(int object_id);
	void remove_object(int object_id);
	void append_objects(std::vector<int>& out);

private:
	std::unordered_set<int> m_objects;
	std::mutex m_mutex;
};

class SectorManager {
public:
	SectorManager();

	void add_object(int object_id, short x, short y);
	void remove_object(int object_id, short x, short y);
	void update_object(int object_id, short old_x, short old_y, short new_x, short new_y);
	std::vector<int> get_objects_in_adjacent_sectors(short x, short y);

private:
	static int sector_id(short x, short y);

	std::array<std::unique_ptr<Sector>, MAX_SECTORS_X * MAX_SECTORS_Y> m_sectors;
};
