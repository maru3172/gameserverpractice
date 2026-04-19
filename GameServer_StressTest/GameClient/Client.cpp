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
			if (event.type == sf::Event::KeyPressed)
			{
				DIRECTION direction;
				switch (event.key.code)
				{
				case sf::Keyboard::Left: direction = LEFT; break;
				case sf::Keyboard::Right: direction = RIGHT; break;
				case sf::Keyboard::Up: direction = UP; break;
				case sf::Keyboard::Down: direction = DOWN; break;
				case sf::Keyboard::Escape: window.close(); client_finish(); exit(0); break;
				}
				if (-1 != direction)
				{
					C2S_Move p;
					p.size = sizeof(p);
					p.type = C2S_MOVE;
					p.dir = direction;
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