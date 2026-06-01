#include "pch.h"
#include "SECTOR.h"

void SECTOR::add_object(int object_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_objects.insert(object_id);
}

void SECTOR::remove_object(int object_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_objects.erase(object_id);
}
