#pragma once

class OBJECT
{
public:
	OBJECT() { m_showing = false; }

	OBJECT(sf::Texture& t, int x, int y, int x2, int y2);

	void show() { m_showing = true; }
	void hide() { m_showing = false; }
	void a_move(int x, int y) { m_sprite.setPosition((float)x, (float)y); }
	void a_draw() { g_window->draw(m_sprite); }
	void move(int x, int y) { m_x = x; m_y = y; }

	void draw();
	void set_name(const char str[]);

	int m_x, m_y;
	char name[MAX_NAME_LEN];

private:
	bool m_showing;
	sf::Sprite m_sprite;
	sf::Text m_name;
};

