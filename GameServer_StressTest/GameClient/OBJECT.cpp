#include "pch.h"
#include "OBJECT.h"

OBJECT::OBJECT(sf::Texture& t, int x, int y, int x2, int y2)
{
	m_showing = false;
	m_sprite.setTexture(t);
	m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
}

void OBJECT::draw()
{
	if (!m_showing) return;
	float rx = (m_x - g_left_x) * 65.0f + 1;
	float ry = (m_y - g_top_y) * 65.0f + 1;
	m_sprite.setPosition(rx, ry);
	g_window->draw(m_sprite);
	auto size = m_name.getGlobalBounds();
	m_name.setPosition(rx + 32 - size.width / 2, ry - 10);
	g_window->draw(m_name);
}

void OBJECT::set_name(const char str[])
{
	m_name.setFont(*g_font);
	m_name.setString(str);
	m_name.setCharacterSize(20);
	m_name.setFillColor(sf::Color(255, 255, 0));
	m_name.setStyle(sf::Text::Bold);
}
