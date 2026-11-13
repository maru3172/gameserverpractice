#include "GameServer.h"
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);
	std::cout << msg;
	std::wcout << L" [" << err_no << L"] " << (lpMsgBuf ? lpMsgBuf : L"") << std::endl;
	LocalFree(lpMsgBuf);
}

bool GameServer::initialize(unsigned short port, int num_workers)
{
	m_num_workers = num_workers;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

	m_listen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	if (SOCKET_ERROR == ::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
		error_display("bind Error :", WSAGetLastError());
		return false;
	}
	if (SOCKET_ERROR == listen(m_listen, SOMAXCONN)) {
		error_display("listen Error :", WSAGetLastError());
		return false;
	}

	m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	std::cout << "Protobuf IOCP server listening on port " << port << ".\n";
	return true;
}

std::shared_ptr<Session> GameServer::get_session(int id)
{
	if (id < 0 || id >= NPC_ID_START) return nullptr;
	return m_sessions[id].load();
}

int GameServer::new_session_id()
{
	int id;
	if (m_free_ids.try_pop(id)) return id;
	id = m_next_id++;
	return id < NPC_ID_START ? id : -1;
}

void GameServer::release_session(int id)
{
	if (id < 0 || id >= NPC_ID_START) return;
	if (m_sessions[id].exchange(nullptr) != nullptr)
		m_free_ids.push(id);
}

void GameServer::run()
{
	std::vector<std::thread> workers;
	for (int i = 0; i < m_num_workers; ++i)
		workers.emplace_back([this] { worker_thread(); });

	for (;;) {
		SOCKADDR_IN client_addr{};
		int addr_len = sizeof(client_addr);
		SOCKET client = accept(m_listen, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
		if (INVALID_SOCKET == client) {
			error_display("Accept Error :", WSAGetLastError());
			continue;
		}

		int id = new_session_id();
		if (id < 0) {
			std::cout << "Server is full.\n";
			closesocket(client);
			continue;
		}

		BOOL no_delay = TRUE;
		setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&no_delay), sizeof(no_delay));

		auto session = std::make_shared<Session>(client, id);
		m_sessions[id].store(session);
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(client), m_iocp, id, 0);
		if (!session->start_recv()) {
			disconnect(id);
			release_session(id);
		}
	}
}

void GameServer::worker_thread()
{
	for (;;) {
		DWORD num_bytes = 0;
		ULONG_PTR key = 0;
		WSAOVERLAPPED* p_over = nullptr;
		BOOL ok = GetQueuedCompletionStatus(m_iocp, &num_bytes, &key, &p_over, INFINITE);
		int id = static_cast<int>(key);
		OVER_EX* over_ex = reinterpret_cast<OVER_EX*>(p_over);

		if (nullptr == over_ex) {
			error_display("GQCS Error :", WSAGetLastError());
			continue;
		}

		if (EV_SEND == over_ex->event_type) {
			delete over_ex;
			if (FALSE == ok) disconnect(id);
			continue;
		}

		// EV_RECV
		if (FALSE == ok || 0 == num_bytes) {
			disconnect(id);
			release_session(id);
			continue;
		}
		on_recv(id, num_bytes);
	}
}

void GameServer::on_recv(int id, DWORD num_bytes)
{
	auto session = get_session(id);
	if (!session) return;

	bool ok = session->on_recv(num_bytes,
		[this, &session](unsigned char type, const unsigned char* data, int size) {
			return process_packet(*session, type, data, size);
		});

	if (!ok || !session->start_recv()) {
		disconnect(id);
		release_session(id);
	}
}

bool GameServer::process_packet(Session& session, unsigned char type, const unsigned char* data, int size)
{
	switch (type) {
	case PKID_CS_LOGIN: {
		CS_LOGIN p;
		if (!p.ParseFromArray(data, size)) return false;
		process_login(session, p.user_name());
		return true;
	}
	case PKID_CS_MOVE: {
		CS_MOVE p;
		if (!p.ParseFromArray(data, size)) return false;
		if (session.m_state != ST_INGAME) return true;
		session.m_move_time = p.move_time();
		process_move(session, p.dir());
		return true;
	}
	default:
		std::cout << "Invalid packet type [" << int(type) << "] from client " << session.id() << "\n";
		return false;
	}
}

void GameServer::process_login(Session& session, const std::string& name)
{
	SESSION_STATE expected = ST_CONNECTED;
	if (!session.m_state.compare_exchange_strong(expected, ST_INGAME)) return;

	session.m_name = name.substr(0, MAX_ID_LEN);
	m_sectors.add_object(session.id(), session.m_x, session.m_y);
	session.send_login_ok();
	session.send_put_object(session);

	for (int other_id : m_sectors.get_objects_in_adjacent_sectors(session.m_x, session.m_y)) {
		if (other_id == session.id()) continue;
		auto other = get_session(other_id);
		if (!other || other->m_state != ST_INGAME) continue;
		if (!session.can_see(*other)) continue;
		other->send_put_object(session);
		session.send_put_object(*other);
	}
}

void GameServer::process_move(Session& session, int dir)
{
	short x = session.m_x;
	short y = session.m_y;
	switch (dir) {
	case Direction::UP:    if (y > 0) y--; break;
	case Direction::DOWN:  if (y < WORLD_HEIGHT - 1) y++; break;
	case Direction::LEFT:  if (x > 0) x--; break;
	case Direction::RIGHT: if (x < WORLD_WIDTH - 1) x++; break;
	case DIR_TELEPORT:
		x = static_cast<short>(rand() % WORLD_WIDTH);
		y = static_cast<short>(rand() % WORLD_HEIGHT);
		break;
	default:
		std::cout << "Invalid direction " << dir << " from client " << session.id() << "\n";
		return;
	}

	short old_x = session.m_x, old_y = session.m_y;
	session.m_x = x;
	session.m_y = y;
	m_sectors.update_object(session.id(), old_x, old_y, x, y);

	std::unordered_set<int> old_view = session.copy_view();
	std::unordered_set<int> new_view;
	for (int other_id : m_sectors.get_objects_in_adjacent_sectors(x, y)) {
		if (other_id == session.id()) continue;
		auto other = get_session(other_id);
		if (!other || other->m_state != ST_INGAME) continue;
		if (session.can_see(*other)) new_view.insert(other_id);
	}

	session.send_pos(session);

	for (int other_id : old_view) {
		auto other = get_session(other_id);
		if (new_view.count(other_id)) {
			if (other) other->send_pos(session);
		}
		else {
			session.send_remove_object(other_id);
			if (other) other->send_remove_object(session.id());
		}
	}
	for (int other_id : new_view) {
		if (old_view.count(other_id)) continue;
		auto other = get_session(other_id);
		if (!other) continue;
		session.send_put_object(*other);
		other->send_put_object(session);
	}
}

void GameServer::disconnect(int id)
{
	auto session = get_session(id);
	if (!session) return;

	SESSION_STATE old_state = session->m_state.exchange(ST_FREE);
	if (old_state == ST_FREE) return;

	if (old_state == ST_INGAME)
		m_sectors.remove_object(id, session->m_x, session->m_y);

	for (int other_id : session->take_view()) {
		if (auto other = get_session(other_id))
			other->send_remove_object(id);
	}
	session->close_socket();
}
