#include "pch.h"
#include "SESSION.h"

void send_login_fail(SOCKET client, const char* message)
{
	// �÷��̾� �α��� ���� �� ����
	S2C_LoginResult packet;
	packet.size = sizeof(S2C_LoginResult);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strncpy_s(packet.message, message, sizeof(packet.message));
	WSABUF wsa_buf;
	wsa_buf.buf = reinterpret_cast<char*>(&packet);
	wsa_buf.len = packet.size;
	WSASend(client, &wsa_buf, 1, 0, 0, nullptr, nullptr);
}

int main()
{
	// ���� �ʱ�ȭ, IOCP ���� �� ���, Ŭ���̾�Ʈ ���� ���
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);
	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)server, h_iocp, -1, 0);

	// ù Ŭ���̾�Ʈ ���� ��� �ʱ�ȭ
	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	EXP_OVER accept_over(IO_ACCEPT);
	AcceptEx(server, client_socket, &accept_over.m_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over.m_over);  // �񵿱�� ù ���� ���

	for (int player_index = 0;;)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		LPOVERLAPPED over;
		// IOCP ���� �� IO(�񵿱�) �۾� �Ϸ�� ���� �޾ƿ��� �׷��� ������ ������ ���
		GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		// ���� ���� ->
		if (over == nullptr) {
			error_display(L"GQCS Errror: ", WSAGetLastError());
			if (key == -1) { // ���� ��ü���� ���� �߻�
				exit(-1);
			}
			// Ŭ���̾�Ʈ ���� ����
			std::cout << "client[" << key << "] Disconnected.\n";
			clients[key].m_is_connected = false;
			for (auto& cl : clients)
				if (true == cl.m_is_connected) cl.send_remove_player(key);
			closesocket(clients[key].m_client);
			clients[key].m_client = INVALID_SOCKET;
			continue;
		}
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);
		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			std::cout << "Client connected." << std::endl;

			player_index = -1;
			// ��� �ִ� Ŭ���̾�Ʈ ã��, �ִٸ� �÷��̾� �ε��� ����
			for (int i = 0; i < MAX_PLAYERS; ++i) {
				if (!clients[i].m_is_connected) {
					player_index = i;
					break;
				}
			}

			// ������ �ο��� �� �� �־� ���� �Ұ���
			if (-1 == player_index) {
				std::cout << "No more player can be accepted." << std::endl;
				send_login_fail(client_socket, "Server is full.");
				closesocket(client_socket);
			}
			else { // �� ���� ������ Ŭ���̾�Ʈ ��� �� �ʱ�ȭ
				CreateIoCompletionPort((HANDLE)client_socket, h_iocp, player_index, 0);
				clients[player_index].m_is_connected = true;
				clients[player_index].m_client = client_socket;
				clients[player_index].m_x = 0;
				clients[player_index].m_y = 0;
				clients[player_index].m_id = player_index;
				clients[player_index].send_login_success(); // ���� ���� �˸�
				clients[player_index].m_prev_recv = 0;

				clients[player_index].do_recv(); // ���� ����
			}
			// ���ӽ� �ش� ���Ͽ� Ŭ���̾�Ʈ�� ����������
			client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // ���� �� Ŭ���̾�Ʈ ���� ����
			AcceptEx(server, client_socket, &accept_over.m_buff, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				NULL, &accept_over.m_over); // �񵿱�� ���� ���� ���
			// ���� ��� -> AcceptEx�� ��� ��� -> ������ ����ϴ� IO�۾��� �Ϸ�Ǹ� GetQueuedCompletionStatus���� �޾ƿ�
		}
		break;
		case IO_RECV:
		{
			// �÷��̾� �ε��� �޾ƿ���
			int player_index = static_cast<int>(key);
			// �÷��̾� ���� ����
			if (num_bytes == 0) {
				std::cout << "Client[" << player_index << "] Disconnected.\n";
				clients[player_index].m_is_connected = false;
				// ��ε�ĳ��Ʈ - �ش� �÷��̾� ���� ����� �˸�
				for (auto& cl : clients)
					if (true == cl.m_is_connected) cl.send_remove_player(player_index);
				closesocket(clients[player_index].m_client); // �÷��̾� ���� ����
				clients[player_index].m_client = INVALID_SOCKET;
				continue;
			}

			std::cout << "Client[" << player_index << "] sent a message." << std::endl;
			SESSION& cl = clients[player_index]; // ó���� ������ �÷��̾ �޾ƿ���
			unsigned char* p = reinterpret_cast<unsigned char*>(exp_over->m_buff); // ������ �����͸� ����
			int data_size = num_bytes + cl.m_prev_recv; // ���� �ܿ� Ŭ���̾�Ʈ ���� ��Ŷ ������ + ���� ������ ��Ŷ ������

			while (data_size > 0) {
				int packet_size = p[0];
				if (packet_size > data_size) break; // ���ó� ��ϵ� ��Ŷ ����� �� ũ�� �� ����������
				cl.process_packet(p); // ��Ŷ ó��
				p += packet_size; // �߰��� ó���� ��Ŷ ������ ��ġ �̵�
				data_size -= packet_size; // �� �߰��� ó���ؾ��� ������ �ִ��� �ľ�
			}

			if (data_size > 0) {
				memmove(cl.m_recv_over.m_buff, p, data_size); // �ܿ� ��Ŷ �����͸� �ش� Ŭ���̾�Ʈ ���۷� �Ű� ������ ����
				cl.m_prev_recv = data_size; // �ܿ� ũ�� ����
			}
			cl.do_recv();
		}
		break;
		case IO_SEND: // �۽� �Ϸ��� ���� �޸� ����
		{
			std::cout << "Message sent. to client[" << key << "]\n";
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			delete o;
		}
		break;
		default: // �� �� ���� �̻��� IO Ÿ���� ������
		{
			std::cout << "Unknown IO type." << std::endl;
			exit(-1);
		}
		break;
		}
	}

	closesocket(server);
	WSACleanup();
}