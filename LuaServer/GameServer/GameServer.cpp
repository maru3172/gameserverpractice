#include "pch.h"
#include "SESSION.h"

void timer_thread()
{
	while (true) {
		event_type ev;
		if (timer_queue.try_pop(ev)) {
			auto now = std::chrono::system_clock::now();
			if (ev.wakeup_time <= now) {
				switch (ev.event_id) {
				case EVENT_MOVE:
					EXP_OVER* move_over = new EXP_OVER;
					move_over->m_iotype = IO_NPCMOVE; // 이동 이벤트는 OP_SEND로 처리
					PostQueuedCompletionStatus(g_iocp, -1, ev.obj_id, &move_over->m_over); // 이동 이벤트를 워커 스레드로 전달
					break;
				}
				continue;
			}
			else timer_queue.push(ev); // 아직 시간이 안 됐으면 다시 큐에 넣음
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void worker_thread()
{
	for (;;)
	{
		DWORD num_bytes;
		ULONG_PTR long_key;
		LPOVERLAPPED over;
		BOOL ret = GetQueuedCompletionStatus(g_iocp, &num_bytes, &long_key, &over, INFINITE);
		int key = static_cast<int>(long_key);
		if (TRUE != ret) {
			error_display(L"GQCS Errror: ", WSAGetLastError());
			if (key == -1) {
				exit(-1);
			}
			disconnect(key);
			continue;
		}

		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);
		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			std::cout << "Client connected." << std::endl;
			if (g_num_players >= MAX_PLAYERS) {
				std::cout << "No more player can be accepted." << std::endl;
				send_login_fail(exp_over->m_client_socket, "Server is full.");
				closesocket(exp_over->m_client_socket);
			}
			else {
				g_num_players++;
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
			if (0 == num_bytes) {
				disconnect(key);
				break;
			}
			std::shared_ptr<SESSION> cl = clients[key];
			if (nullptr == cl) {
				std::cout << "Session not found for client[" << player_index << "].\n";
				break;
			}
			unsigned char* p = reinterpret_cast<unsigned char*>(exp_over->m_buff);
			int data_size = num_bytes + cl->m_prev_recv;
			while (data_size > 0) {
				int packet_size = p[0];
				if (packet_size > data_size) break;
				if (false == cl->process_packet(p)) {
					disconnect(key);
					break;
				}
				p += packet_size;
				data_size -= packet_size;
			}
			if (data_size > 0) memmove(cl->m_recv_over.m_buff, p, data_size);
			cl->m_prev_recv = data_size;
			cl->do_recv();
		}
		break;
		case IO_SEND:
		{
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			delete o;
		}
		break;
		case IO_NPCMOVE:
		{
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			delete o;

			int npc_id = static_cast<int>(key);
			std::shared_ptr<SESSION> npc = clients[npc_id];
			if (nullptr != npc) {
				npc->do_random_move();

				// 다음 이동 이벤트 재등록
				event_type ev;
				ev.event_id = EVENT_MOVE;
				ev.obj_id = npc_id;
				ev.target_id = -1;
				ev.wakeup_time = std::chrono::system_clock::now() + std::chrono::milliseconds(MOVE_COOL_TIME);
				timer_queue.push(ev);
			}
		}
		break;
		case IO_PLAYER_MOVE:
		{
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			int player_id = o->m_target_id;
			int npc_id = static_cast<int>(key);
			std::shared_ptr<SESSION> npc = clients[npc_id];
			if (nullptr != npc) {
				auto L = npc->L;
				npc->m_lua_mutex.lock();
				lua_getglobal(L, "event_player_move");
				lua_pushinteger(L, player_id);
				int error = lua_pcall(L, 1, 0, 0);
				if (error != LUA_OK) {
					const char* error_msg = lua_tostring(L, -1);
					std::cerr << "Lua error: " << error_msg << std::endl;
					lua_pop(L, 1); // remove error message from stack
				}
				npc->m_lua_mutex.unlock();
			}
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
	initialize_npcs();

	auto npc_ai = std::thread(timer_thread);
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread);
	for (auto& th : worker_threads)
		th.join();
	npc_ai.join();

	closesocket(g_server);
	WSACleanup();
}