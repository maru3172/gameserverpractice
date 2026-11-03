#include "pch.h"
#include "DrawNConnect.h"

int main()
{
	std::wcout.imbue(std::locale("korean"));
	std::cout << "Enter User Name : ";
	std::cin >> avatar_name;
	sf::Socket::Status status = socket.connect("127.0.0.1", PORT);
	socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		std::wcout << L"서버와 연결할 수 없습니다.\n";
		while (true);
	}

	client_initialize();

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed) window.close();
			bool moved = false;
			if (event.type == sf::Event::KeyPressed) {
				switch (event.key.code) {
				case sf::Keyboard::Left:
					if (avatar.m_x > 0) {
						avatar.m_x--;
						moved = true;
					}
					break;
				case sf::Keyboard::Right:
					if (avatar.m_x < WORLD_WIDTH - 1) {
						avatar.m_x++;
						moved = true;
					}
					break;
				case sf::Keyboard::Up:
					if (avatar.m_y > 0) {
						avatar.m_y--;
						moved = true;
					}
					break;
				case sf::Keyboard::Down:
					if (avatar.m_y < WORLD_WIDTH - 1) {
						avatar.m_y++;
						moved = true;
					}
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
				if (true == moved) {
					C2S_Move p;
					p.size = sizeof(p);
					p.type = C2S_MOVE;
					p.x = avatar.m_x;
					p.y = avatar.m_y;
					p.move_time = 0;	// STRESS TEST가 아니면 0을 보내야 한다.
					send_packet(&p);
					g_left_x = avatar.m_x - SCREEN_WIDTH / 2;
					g_top_y = avatar.m_y - SCREEN_HEIGHT / 2;
				}

			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();
}