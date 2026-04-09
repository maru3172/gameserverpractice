#include "pch.h"
#include "OBJECT.h"

int g_left_x;
int g_top_y;
int g_myid;

sf::TcpSocket socket;
sf::RenderWindow* g_window;
sf::Font* g_font;

OBJECT avatar, players[MAX_USER];
std::string avatar_name;

OBJECT white_tile, black_tile;

sf::Texture* board, * pieces;

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	g_font = new sf::Font;
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	if (false == g_font->loadFromFile("cour.ttf")) {
		std::cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	avatar = OBJECT{ *pieces, 128, 0, 64, 64 };
	avatar.set_name(avatar_name.c_str());
	avatar.move(4, 4);
	for (auto& pl : players) {
		pl = OBJECT{ *pieces, 64, 0, 64, 64 };
	}
}

void client_finish()
{
	socket.setBlocking(true);
	socket.disconnect();

	delete g_font;
	delete board;
	delete pieces;
}

void send_packet(void* packet)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	size_t sent = 0;
	socket.send(packet, p[0], sent);
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case S2C_LOGIN_RESULT:
	{
		// �α��� ��� ��Ŷ���� ��ȯ �� ���� ���� ó��
		S2C_LoginResult* packet = reinterpret_cast<S2C_LoginResult*>(ptr);
		if (packet->success)
		{
			std::cout << "Login Success! : " << packet->message << std::endl;
			C2S_Login p;
			p.size = sizeof(p);
			p.type = C2S_LOGIN;
			strcpy_s(p.username, avatar_name.c_str());
			send_packet(&p); // �۽�
		}
		else // �α��� ���� - ���� ����� ���� ������ �ʿ� ����
		{
			std::cout << "Login Failed! : " << packet->message << std::endl;
			exit(-1);
		}
	}
	break;
	case S2C_AVATAR_INFO: // �ڽ��� ����
	{
		S2C_AvatarInfo* packet = reinterpret_cast<S2C_AvatarInfo*>(ptr);
		g_myid = packet->playerId;
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		avatar.show();
	}
	break;
	case S2C_ADD_PLAYER: // �ٸ� �÷��̾� ���� �� ����
	{
		S2C_AddPlayer* my_packet = reinterpret_cast<S2C_AddPlayer*>(ptr);
		int id = my_packet->playerId;
		if (id >= MAX_USER)
		{
			std::cout << "Too many users! id : " << id << std::endl;
			exit(-1);
		}
		players[id].move(my_packet->x, my_packet->y);
		players[id].set_name(my_packet->username);
		players[id].show();
	}
	break;
	case S2C_MOVE_PLAYER: // �÷��̾� �̵�, ó���� �����͸� �������� �޾Ƽ� �̵���� ����
	{
		S2C_MovePlayer* my_packet = reinterpret_cast<S2C_MovePlayer*>(ptr);
		int other_id = my_packet->playerId;
		if (other_id == g_myid)
		{
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - 4;
			g_top_y = my_packet->y - 4;
		}
		else if (other_id < MAX_USER)
		{
			players[other_id].move(my_packet->x, my_packet->y);
		}
	}
	break;
	case S2C_REMOVE_PLAYER: // �÷��̾� ���� ����
	{
		S2C_RemovePlayer* my_packet = reinterpret_cast<S2C_RemovePlayer*>(ptr);
		int other_id = my_packet->playerId;
		if (other_id == g_myid) avatar.hide();
		else if (other_id < MAX_USER) players[other_id].hide();
	}
	break;
	default: printf("Unknown PACKET type [%d]\n", ptr[1]); break; // ��Ŷ Ÿ�� �� �� ����
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0; // ���� ó������ ��Ŷ ũ��
	static size_t saved_packet_size = 0; // ������ �� �޾Ƽ� �����ص� ������ ũ��
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte)
	{
		if (0 == in_packet_size) in_packet_size = ptr[0]; // ���� ó������ ��Ŷ ����� 0�̶�� �� ��Ŷ��
		if (io_byte + saved_packet_size >= in_packet_size) // ���ݱ��� ������ + �̹��� ���� �����Ͱ� ��Ŷ ũ�� �̻��̶�� ������ ��Ŷ
		{
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size); // ������ ����, ���� ���� �����ŭ�� �����ϸ� ��
			ProcessPacket(packet_buffer); // ��Ŷ ó��
			ptr += in_packet_size - saved_packet_size; // ó���� �����͸�ŭ ������ �Űܰ���
			io_byte -= in_packet_size - saved_packet_size; // ó���� ũ�� ����
			// ����� ���� ���� ũ�� �ʱ�ȭ
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else // �ƴ϶�� ��� ����
		{
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte); // ������ ����, ������ ���, ������ ũ�⸸ŭ
			saved_packet_size += io_byte; // ����
			io_byte = 0; // �̹� ���� ������ �� ���������� �ʱ�ȭ
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t received;

	auto recv_result = socket.receive(net_buf, BUF_SIZE, received); // ������ ����
	if (recv_result == sf::Socket::Error) // ���� ���� - �α� Ȯ��
	{
		std::wcout << L"Recv ����!";
		while (true);
	}

	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
	{
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			if (0 == (i + j) % 2)
			{
				white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				black_tile.a_draw();
			}
		}
	}

	avatar.draw();
	for (auto& pl : players) pl.draw();
}