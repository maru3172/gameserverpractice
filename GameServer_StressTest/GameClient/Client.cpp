#include "pch.h"
#include "DrawNConnect.h"

int main()
{
	std::wcout.imbue(std::locale("korean"));
	sf::Socket::Status status = s_socket.connect("127.0.0.1", PORT);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		std::wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	client_initialize();
	player_name = "P";
	player_name += std::to_string(GetCurrentProcessId());
	avatar.set_name(player_name.c_str());

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed) window.close();
			if (event.type == sf::Event::KeyPressed)
			{
				int direction = -1;
				switch (event.key.code)
				{
				case sf::Keyboard::Left: direction = 2; break;
				case sf::Keyboard::Right: direction = 3; break;
				case sf::Keyboard::Up: direction = 0; break;
				case sf::Keyboard::Down: direction = 1; break;
				case sf::Keyboard::Escape: window.close(); client_finish(); exit(0); break;
				}
				if (-1 != direction)
				{
					C2S_Move p;
					p.size = sizeof(p);
					p.type = C2S_MOVE;
					p.dir = static_cast<DIRECTION>(direction);
					p.move_time = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now().time_since_epoch()).count());
					send_packet(&p);
				}
			}
		}

		window.clear();
		client_main();
		window.display();
	}

	client_finish();
}