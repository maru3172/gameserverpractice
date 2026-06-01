#include "pch.h"
#include "SESSION.h"
#include "SectorManager.h"

void worker_thread()
{
	for (;;) {
		DWORD num_bytes;
		ULONG_PTR long_key;
		LPOVERLAPPED over;
		BOOL ret = GetQueuedCompletionStatus(g_iocp, &num_bytes, &long_key, &over, INFINITE);
		int key = static_cast<int>(long_key);
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);

		if (TRUE != ret) {
			error_display(L"GQCS Error: ", WSAGetLastError());
			if (key >= 0) disconnect(key);
			continue;
		}

		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			int my_id = get_new_player_id();
			if (-1 == my_id) {
				send_login_fail(exp_over->m_client_socket, "Server is full.");
				closesocket(exp_over->m_client_socket);
			}
			else {
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

			std::shared_ptr<SESSION> cl = get_session(key);
			if (nullptr == cl) break;

			unsigned char* p = reinterpret_cast<unsigned char*>(exp_over->m_buff);
			int data_size = num_bytes + cl->m_prev_recv;
			while (data_size > 0) {
				int packet_size = p[0];
				if (packet_size > data_size) break;
				if (!cl->process_packet(p)) {
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
			else cl->m_prev_recv = 0;
			cl->do_recv();
		}
		break;
		case IO_SEND:
		{
			delete exp_over;
		}
		break;
		case IO_NPC_MOVE:
		{
			delete exp_over;
			process_npc_move(key);
		}
		break;
		default:
		{
			std::cout << "Unknown IO type.\n";
		}
		break;
		}
	}
}

void timer_thread()
{
	for (;;) {
		event_type ev;
		if (timer_queue.try_pop(ev)) {
			auto now = std::chrono::system_clock::now();
			if (ev.wakeup_time <= now) {
				if (ev.event_id == EVENT_NPC_MOVE) {
					EXP_OVER* move_over = new EXP_OVER(IO_NPC_MOVE);
					PostQueuedCompletionStatus(g_iocp, 1, ev.obj_id, &move_over->m_over);
				}
			}
			else {
				timer_queue.push(ev);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		else std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void InitializeNPC()
{
	std::cout << "NPC initialize begin.\n";
	for (int i = NPC_ID_START; i < NPC_ID_START + MAX_NPCS; ++i) {
		std::shared_ptr<SESSION> npc = std::make_shared<SESSION>();
		npc->m_id = i;
		npc->m_is_npc = true;
		npc->m_state = CS_PLAYING;
		npc->m_client = INVALID_SOCKET;
		npc->m_x = rand() % WORLD_WIDTH;
		npc->m_y = rand() % WORLD_HEIGHT;
		npc->m_move_time = 0;
		npc->m_active_npc = false;
		npc->m_last_npc_move_time = std::chrono::system_clock::now();
		sprintf_s(npc->m_username, "NPC%d", i);
		clients[i] = npc;
		sector_manager.add_object_to_sector(i, npc->m_x, npc->m_y);
	}
	std::cout << "NPC initialize end.\n";
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

	InitializeNPC();

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)g_server, g_iocp, -1, 0);

	EXP_OVER accept_over(IO_ACCEPT);
	accept_over.m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	AcceptEx(g_server, accept_over.m_client_socket, &accept_over.m_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over.m_over);

	std::vector<std::thread> worker_threads;
	std::thread timer_th(timer_thread);
	unsigned int hw_threads = std::thread::hardware_concurrency();
	int num_threads = hw_threads > 1 ? static_cast<int>(hw_threads - 1) : 1;
	for (int i = 0; i < num_threads; ++i) worker_threads.emplace_back(worker_thread);

	for (auto& th : worker_threads) th.join();
	timer_th.join();

	closesocket(g_server);
	WSACleanup();
}