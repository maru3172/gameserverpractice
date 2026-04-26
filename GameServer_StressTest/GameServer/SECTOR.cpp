#include "pch.h"
#include "SECTOR.h"

SECTOR::SECTOR()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_players.clear();
}

SECTOR::~SECTOR()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_players.clear();
}

void SECTOR::add_player(int player_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_players.insert(player_id);
}

void SECTOR::remove_player(int player_id)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_players.erase(player_id);
}
