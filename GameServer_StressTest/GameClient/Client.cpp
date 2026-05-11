#include "pch.h"
#include "DrawNConnect.h"

int main()
{
	std::wcout.imbue(std::locale("korean"));
	sf::Socket::Status status = s_socket.connect("127.0.0.1", PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		std::wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;

	std::string player_name{ "P" };
	player_name += std::to_string(GetCurrentProcessId());

	strcpy_s(p.name, player_name.c_str());
	send_packet(&p);
	avatar.set_name(p.name);

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
					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
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