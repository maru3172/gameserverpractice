#include "pch.h"
#include "SESSION.h"

void worker_thread()
{
	for (;;)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		LPOVERLAPPED over;
		GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		if (over == nullptr)
		{
			error_display(L"GQCS Errror: ", WSAGetLastError());
			if (key == -1) exit(-1);
			std::cout << "client[" << key << "] Disconnected.\n";
			std::shared_ptr<SESSION> cl = clients[key].load();
			if (nullptr != cl)
			{
				cl->m_state = CS_LOGOUT;
				for (auto& other : clients) {
					std::shared_ptr<SESSION> o = other.second.load();
					if (nullptr == o) continue;
					if (CS_PLAYING == o->m_state) o->send_remove_player(key);
				}
				closesocket(cl->m_client);
				cl->m_client = INVALID_SOCKET;
			}
			clients[key].store(nullptr);
			continue;
		}
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);

		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			std::cout << "Client connected." << std::endl;
			//client_socket = exp_over->m_client_socket; <- 안씀
			if (MAX_PLAYERS <= clients.size()) // 클라 가득 참
			{
				std::cout << "No more player can be accepted." << std::endl;
				send_login_fail(exp_over->m_client_socket, "Server is full.");
				closesocket(exp_over->m_client_socket);
			}
			else // 아니라면 일단 서버에 입장
			{
				int my_id = player_index++;
				CreateIoCompletionPort((HANDLE)exp_over->m_client_socket, h_iocp, my_id, 0);
				std::shared_ptr<SESSION> new_pl = std::make_shared<SESSION>(exp_over->m_client_socket, my_id);
				clients[my_id] = new_pl;
				new_pl->send_login_success();
				new_pl->do_recv();
			}
			exp_over->m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			EXP_OVER accept_over(IO_ACCEPT);
			AcceptEx(server, exp_over->m_client_socket, &accept_over.m_buff, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				NULL, &accept_over.m_over);
		}
		break;
		case IO_RECV:
		{
			int player_index = static_cast<int>(key);
			std::cout << "Client[" << player_index << "] sent a message.\n";
			SESSION& cl = *clients[player_index].load();
			unsigned char* p = reinterpret_cast<unsigned char*>(exp_over->m_buff);
			if (num_bytes == 0) // 클라이언트 접속 종료
			{
				std::cout << "Client[" << player_index << "] Disconnected.\n";
				cl.m_state = CS_LOGOUT;
				for (auto& other : clients) {
					std::shared_ptr<SESSION> o = other.second.load();
					if (nullptr == o) continue;
					if (CS_PLAYING == o->m_state) o->send_remove_player(player_index);
				}
				closesocket(cl.m_client);
				cl.m_client = INVALID_SOCKET;
				clients[player_index].store(nullptr);
				continue;
			}
			int data_size = num_bytes + cl.m_prev_recv;
			while (data_size > 0) {
				int packet_size = p[0];
				if (packet_size > data_size) break;
				cl.process_packet(p);
				p += packet_size;
				data_size -= packet_size;
			}
			if (data_size > 0) {
				memmove(cl.m_recv_over.m_buff, p, data_size);
				cl.m_prev_recv = data_size;
			}
			cl.do_recv();
		}
		break;
		case IO_SEND:
		{
			std::cout << "Message sent. to client[" << key << "]\n";
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
	server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	::bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)server, h_iocp, -1, 0);

	EXP_OVER accept_over(IO_ACCEPT);
	accept_over.m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	AcceptEx(server, accept_over.m_client_socket, &accept_over.m_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over.m_over);

	std::vector<std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	worker_threads.reserve(num_threads);

	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread);
	for (auto& th : worker_threads)
		th.join();

	closesocket(server);
	WSACleanup();
}