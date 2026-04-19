#include "pch.h"
#include "SESSION.h"

void worker_thread()
{
	for (;;)
	{
		DWORD num_bytes;
		ULONG_PTR long_key;
		LPOVERLAPPED over;
		BOOL ret = GetQueuedCompletionStatus(g_iocp, &num_bytes, &long_key, &over, INFINITE);
		int key = static_cast<int>(long_key);
		if (TRUE != ret)
		{
			error_display(L"GQCS Errror: ", WSAGetLastError());
			if (key == -1) exit(-1);
			disconnect(key);
			continue;
		}

		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);
		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			std::cout << "Client connected.\n";
			if (MAX_PLAYERS <= clients.size()) // 연결은 됬으나 꽉참
			{
				std::cout << "No more player can be accepted." << std::endl;
				send_login_fail(exp_over->m_client_socket, "Server is full.");
				closesocket(exp_over->m_client_socket);
			}
			else // 들어올 수 있음
			{
				int my_id = player_index++;
				CreateIoCompletionPort((HANDLE)exp_over->m_client_socket, g_iocp, my_id, 0);
				std::shared_ptr<SESSION> new_pl = std::make_shared<SESSION>(exp_over->m_client_socket, my_id);
				clients[my_id] = new_pl;
				new_pl->send_login_success();
				new_pl->do_recv();
			}
			exp_over->m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			AcceptEx(g_server, exp_over->m_client_socket, &exp_over->m_buff, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				NULL, &exp_over->m_over);
		}
		break;
		case IO_RECV:
		{
			if (0 == num_bytes) { // 안 보내니 이제 접속 종료
				disconnect(key);
				break;
			}

			std::shared_ptr<SESSION> cl = clients[key].load();
			if (nullptr == cl) { // 존재하지 않음
				std::cout << "Session not found for client[" << player_index << "].\n";
				break;
			}

			unsigned char* p = reinterpret_cast<unsigned char*>(exp_over->m_buff);
			int data_size = num_bytes + cl->m_prev_recv;
			while (data_size > 0) {
				int packet_size = p[0];
				if (packet_size > data_size) break;
				if (!cl->process_packet(p)) { // 플레이어로부터 알 수 없는 패킷이 전송됨, 위험
					disconnect(key);
					break;
				}
				p += packet_size;
				data_size -= packet_size;
			}
			if (data_size > 0) {
				memmove(cl->m_recv_over.m_buff, p, data_size);
				cl->m_prev_recv = data_size;
			}
			cl->do_recv();
		}
		break;
		case IO_SEND:
		{
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			delete o;
		}
		break;
		default:
		{
			std::cout << "Unknown IO type.\n";
			exit(-1);
		}
		break;
		}
	}
}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(g_server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_server, SOMAXCONN);
	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)g_server, g_iocp, -1, 0);

	EXP_OVER accept_over(IO_ACCEPT);
	accept_over.m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	AcceptEx(g_server, accept_over.m_client_socket, &accept_over.m_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over.m_over);

	std::vector<std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	worker_threads.reserve(num_threads);

	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread);
	for (auto& th : worker_threads)
		th.join();

	closesocket(g_server);
	WSACleanup();
}