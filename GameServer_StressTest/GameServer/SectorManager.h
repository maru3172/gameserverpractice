#pragma once

class SECTOR;

// 섹터 관리, 플레이어 추가 및 삭제
class SectorManager
{
public:
	SectorManager();

	void add_object_to_sector(int object_id, short x, short y);
	void remove_object_from_sector(int object_id, short x, short y);
	void update_object_sector(int object_id, short old_x, short old_y, short new_x, short new_y);
	std::vector<int> get_objects_in_adjacent_sectors(short x, short y);

private:
	int sector_id(short x, short y);

	std::array<std::shared_ptr<SECTOR>, MAX_SECTORS_X * MAX_SECTORS_Y> sectors;
};

