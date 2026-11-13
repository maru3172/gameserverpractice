#include "Session.h"
#include <iostream>

Session::Session(SOCKET socket, int id) : m_socket(socket), m_id(id)
{
	m_x = static_cast<short>(rand() % WORLD_WIDTH);
	m_y = static_cast<short>(rand() % WORLD_HEIGHT);
}

Session::~Session()
{
	close_socket();
}

void Session::close_socket()
{
	SOCKET s = m_socket;
	m_socket = INVALID_SOCKET;
	if (s != INVALID_SOCKET) closesocket(s);
}

bool Session::start_recv()
{
	ZeroMemory(&m_recv_over.over, sizeof(m_recv_over.over));
	m_recv_over.wsabuf.buf = m_recv_over.net_buf;
	m_recv_over.wsabuf.len = MAX_BUFFER;
	DWORD flags = 0;
	int ret = WSARecv(m_socket, &m_recv_over.wsabuf, 1, NULL, &flags, &m_recv_over.over, NULL);
	if (ret == SOCKET_ERROR) {
		int err_no = WSAGetLastError();
		if (err_no != WSA_IO_PENDING) return false;
	}
	return true;
}

bool Session::can_see(const Session& other) const
{
	return abs(m_x - other.m_x) <= VIEW_RANGE && abs(m_y - other.m_y) <= VIEW_RANGE;
}

bool Session::in_view(int id)
{
	std::lock_guard<std::mutex> lock(m_view_lock);
	return m_view.count(id) != 0;
}

std::unordered_set<int> Session::copy_view()
{
	std::lock_guard<std::mutex> lock(m_view_lock);
	return m_view;
}

std::unordered_set<int> Session::take_view()
{
	std::lock_guard<std::mutex> lock(m_view_lock);
	std::unordered_set<int> result;
	result.swap(m_view);
	return result;
}

void Session::send_packet(unsigned char type, const google::protobuf::MessageLite& msg)
{
	if (m_socket == INVALID_SOCKET) return;

	size_t body = msg.ByteSizeLong();
	size_t total = body + PACKET_HEADER_SIZE;
	if (total > MAX_PACKET_SIZE) {
		std::cout << "Packet too large: " << total << "\n";
		return;
	}

	OVER_EX* send_over = new OVER_EX(EV_SEND);
	send_over->net_buf[0] = static_cast<char>(total);
	send_over->net_buf[1] = static_cast<char>(type);
	msg.SerializeToArray(send_over->net_buf + PACKET_HEADER_SIZE, static_cast<int>(body));
	send_over->wsabuf.len = static_cast<ULONG>(total);

	int ret = WSASend(m_socket, &send_over->wsabuf, 1, NULL, 0, &send_over->over, NULL);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		delete send_over;     // 완료 통지가 오지 않음. 연결 종료는 수신 쪽에서 처리된다.
}

void Session::send_login_ok()
{
	SC_LOGIN_OK packet;
	packet.set_id(m_id);
	packet.set_x(m_x);
	packet.set_y(m_y);
	send_packet(PKID_SC_LOGIN_OK, packet);
}

void Session::send_put_object(const Session& obj)
{
	if (obj.m_id != m_id) {
		std::lock_guard<std::mutex> lock(m_view_lock);
		if (!m_view.insert(obj.m_id).second) return;
	}

	SC_PUT_OBJECT packet;
	packet.set_id(obj.m_id);
	packet.set_x(obj.m_x);
	packet.set_y(obj.m_y);
	packet.set_o_type(1);
	send_packet(PKID_SC_PUT_OBJECT, packet);
}

void Session::send_pos(const Session& mover)
{
	if (mover.m_id != m_id && !in_view(mover.m_id)) {
		send_put_object(mover);
		return;
	}

	SC_POS packet;
	packet.set_id(mover.m_id);
	packet.set_x(mover.m_x);
	packet.set_y(mover.m_y);
	packet.set_move_time(mover.m_move_time);
	send_packet(PKID_SC_POS, packet);
}

void Session::send_remove_object(int obj_id)
{
	{
		std::lock_guard<std::mutex> lock(m_view_lock);
		if (m_view.erase(obj_id) == 0) return;
	}

	SC_REMOVE_OBJECT packet;
	packet.set_id(obj_id);
	send_packet(PKID_SC_REMOVE_OBJECT, packet);
}
