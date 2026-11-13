#include "GameServer.h"
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")

using namespace std::chrono;

void error_display(const wchar_t* msg, int err_no)
{
	WCHAR* lpMsgBuf = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);
	std::wcout << msg << L" === error [" << err_no << L"] " << (lpMsgBuf ? lpMsgBuf : L"") << std::endl;
	LocalFree(lpMsgBuf);
}

GameServer& GameServer::instance()
{
	static GameServer server;
	return server;
}

std::shared_ptr<Session> GameServer::get_session(int id)
{
	auto iter = m_clients.find(id);
	if (iter == m_clients.end()) return nullptr;
	return iter->second.load();
}

bool GameServer::initialize()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

	m_listen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == ::bind(m_listen, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr))) {
		error_display(L"bind", WSAGetLastError());
		return false;
	}
	if (SOCKET_ERROR == listen(m_listen, SOMAXCONN)) {
		error_display(L"listen", WSAGetLastError());
		return false;
	}

	initialize_npcs();

	m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listen), m_iocp, static_cast<ULONG_PTR>(-1), 0);
	post_accept();

	std::cout << "Server listening on port " << PORT << ".\n";
	return true;
}

void GameServer::initialize_npcs()
{
	std::cout << "NPC initialize begin.\n";
	for (int i = NPC_ID_START; i < NPC_ID_START + MAX_NPCS; ++i) {
		auto npc = std::make_shared<Session>();
		npc->m_id = i;
		npc->m_state = CS_PLAYING;
		npc->m_x = static_cast<short>(rand() % WORLD_WIDTH);
		npc->m_y = static_cast<short>(rand() % WORLD_HEIGHT);
		sprintf_s(npc->m_username, "NPC%d", i);
		m_clients[i] = npc;
		m_sectors.add_object(i, npc->m_x, npc->m_y);
	}
	std::cout << "NPC initialize end.\n";
}

void GameServer::post_accept()
{
	m_accept_over.m_client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	ZeroMemory(&m_accept_over.m_over, sizeof(m_accept_over.m_over));
	BOOL ok = AcceptEx(m_listen, m_accept_over.m_client_socket, m_accept_over.m_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &m_accept_over.m_over);
	if (FALSE == ok && WSAGetLastError() != ERROR_IO_PENDING)
		error_display(L"AcceptEx", WSAGetLastError());
}

int GameServer::get_new_player_id()
{
	int id;
	if (m_free_ids.try_pop(id)) return id;
	id = m_next_player_id++;
	if (id >= NPC_ID_START) return -1;
	return id;
}

void GameServer::release_session(int key)
{
	if (!is_pc(key)) return;
	auto iter = m_clients.find(key);
	if (iter == m_clients.end()) return;
	if (iter->second.exchange(nullptr) != nullptr)
		m_free_ids.push(key);
}

void GameServer::send_server_full(SOCKET client)
{
	S2C_LoginResult packet{};
	packet.size = sizeof(packet);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strcpy_s(packet.message, "Server is full.");
	send(client, reinterpret_cast<char*>(&packet), packet.size, 0);
}

void GameServer::update_player_view(int player_id)
{
	auto player = get_session(player_id);
	if (nullptr == player || !player->can_send()) return;

	std::unordered_set<int> old_view = player->copy_view_list();

	std::unordered_set<int> new_view;
	for (int object_id : m_sectors.get_objects_in_adjacent_sectors(player->m_x, player->m_y)) {
		if (object_id == player_id) continue;
		auto obj = get_session(object_id);
		if (nullptr == obj || obj->m_state != CS_PLAYING) continue;
		if (player->can_see(obj->m_x, obj->m_y))
			new_view.insert(object_id);
	}

	player->send_move_object(player_id);

	for (int object_id : new_view) {
		auto obj = get_session(object_id);
		if (nullptr == obj) continue;

		if (old_view.count(object_id) == 0) {
			player->send_add_object(object_id);
			if (is_pc(object_id))
				obj->send_add_object(player_id);
			else
				obj->wake_up();
		}
		else if (is_pc(object_id)) {
			if (obj->is_visible(player_id))
				obj->send_move_object(player_id);
			else
				obj->send_add_object(player_id);
		}
	}

	for (int object_id : old_view) {
		if (new_view.count(object_id) != 0) continue;
		player->send_remove_object(object_id);
		if (is_pc(object_id)) {
			auto other = get_session(object_id);
			if (nullptr != other)
				other->send_remove_object(player_id);
		}
	}
}

void GameServer::disconnect(int key)
{
	auto cl = get_session(key);
	if (nullptr == cl || !is_pc(key)) return;

	CL_STATE old_state = cl->m_state.exchange(CS_LOGOUT);
	if (old_state == CS_LOGOUT || old_state == CS_FREE) return;

	if (old_state == CS_PLAYING)
		m_sectors.remove_object(key, cl->m_x, cl->m_y);

	for (int object_id : cl->take_view_list()) {
		if (!is_pc(object_id)) continue;
		auto other = get_session(object_id);
		if (nullptr != other)
			other->send_remove_object(key);
	}
	cl->close_socket();
}

void GameServer::on_accept(EXP_OVER* over)
{
	SOCKET client = over->m_client_socket;
	int my_id = get_new_player_id();
	if (-1 == my_id) {
		send_server_full(client);
		closesocket(client);
	}
	else {
		setsockopt(client, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			reinterpret_cast<char*>(&m_listen), sizeof(m_listen));
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(client), m_iocp, my_id, 0);
		auto new_pl = std::make_shared<Session>(client, my_id);
		m_clients[my_id] = new_pl;
		if (!new_pl->do_recv()) {
			disconnect(my_id);
			release_session(my_id);
		}
	}
	post_accept();
}

void GameServer::on_recv(int key, DWORD num_bytes)
{
	auto cl = get_session(key);
	if (nullptr == cl) return;

	if (0 == num_bytes) {
		disconnect(key);
		release_session(key);
		return;
	}

	unsigned char* p = reinterpret_cast<unsigned char*>(cl->m_recv_over.m_buff);
	int data_size = static_cast<int>(num_bytes) + cl->m_prev_recv;
	while (data_size > 0) {
		int packet_size = p[0];
		if (packet_size < 2) {     // 잘못된 패킷 (0 이면 무한 루프)
			disconnect(key);
			release_session(key);
			return;
		}
		if (packet_size > data_size) break;
		if (!cl->process_packet(p)) {
			disconnect(key);
			release_session(key);
			return;
		}
		p += packet_size;
		data_size -= packet_size;
	}

	if (data_size > 0)
		memmove(cl->m_recv_over.m_buff, p, data_size);
	cl->m_prev_recv = data_size;

	if (!cl->do_recv()) {
		disconnect(key);
		release_session(key);
	}
}

void GameServer::on_db_login(int key, EXP_OVER* over)
{
	auto cl = get_session(key);
	if (nullptr == cl || cl->m_state != CS_DB_WAIT) return;

	const DB_RESULT& result = over->m_db_result;
	if (!result.success) {
		cl->send_login_fail();
		cl->m_state = CS_CONNECT;     // 다시 로그인 시도 가능
		return;
	}

	cl->m_db_user_id = result.user_id;
	sprintf_s(cl->m_username, "%d", cl->m_db_user_id);
	if (result.has_position) {
		cl->m_x = result.x;
		cl->m_y = result.y;
	}
	else {
		m_db.request_save_position(cl->m_db_user_id, cl->m_x, cl->m_y);
	}

	CL_STATE expected = CS_DB_WAIT;
	if (!cl->m_state.compare_exchange_strong(expected, CS_PLAYING)) return;

	m_sectors.add_object(cl->m_id, cl->m_x, cl->m_y);
	cl->send_login_success();
	cl->send_avatar_info();
	update_player_view(cl->m_id);
}

void GameServer::on_npc_move(int npc_id)
{
	auto npc = get_session(npc_id);
	if (nullptr == npc || !is_npc(npc_id) || npc->m_state != CS_PLAYING) return;

	npc->do_random_move();

	bool has_nearby_player = false;
	for (int object_id : m_sectors.get_objects_in_adjacent_sectors(npc->m_x, npc->m_y)) {
		if (!is_pc(object_id)) continue;
		auto player = get_session(object_id);
		if (nullptr != player && player->can_send() && player->can_see(npc->m_x, npc->m_y)) {
			has_nearby_player = true;
			break;
		}
	}

	if (has_nearby_player)
		m_timer.add_event(npc_id, EV_NPC_MOVE, milliseconds(MOVE_COOL_TIME));
	else
		npc->m_active_npc = false;
}

void GameServer::worker_thread()
{
	for (;;) {
		DWORD num_bytes = 0;
		ULONG_PTR long_key = 0;
		LPOVERLAPPED over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(m_iocp, &num_bytes, &long_key, &over, INFINITE);
		int key = static_cast<int>(long_key);
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);

		if (nullptr == exp_over) {
			error_display(L"GQCS Error: ", WSAGetLastError());
			continue;
		}

		if (FALSE == ret) {
			switch (exp_over->m_iotype) {
			case IO_SEND:
				delete exp_over;
				disconnect(key);
				break;
			case IO_RECV:
				disconnect(key);
				release_session(key);
				break;
			case IO_ACCEPT:
				closesocket(exp_over->m_client_socket);
				post_accept();
				break;
			default:
				delete exp_over;
				break;
			}
			continue;
		}

		switch (exp_over->m_iotype) {
		case IO_ACCEPT:
			on_accept(exp_over);
			break;
		case IO_RECV:
			on_recv(key, num_bytes);
			break;
		case IO_SEND:
			delete exp_over;
			break;
		case IO_NPC_MOVE:
			delete exp_over;
			on_npc_move(key);
			break;
		case IO_DB_LOGIN:
			on_db_login(key, exp_over);
			delete exp_over;
			break;
		default:
			std::cout << "Unknown IO type.\n";
			break;
		}
	}
}

void GameServer::run()
{
	std::thread timer_th([this] { m_timer.run(m_iocp); });
	std::thread db_th([this] { m_db.run(m_iocp); });

	unsigned int hw_threads = std::thread::hardware_concurrency();
	int num_threads = hw_threads > 1 ? static_cast<int>(hw_threads - 1) : 1;
	std::vector<std::thread> workers;
	for (int i = 0; i < num_threads; ++i)
		workers.emplace_back([this] { worker_thread(); });

	for (auto& th : workers) th.join();
	timer_th.join();
	db_th.join();

	closesocket(m_listen);
	WSACleanup();
}
