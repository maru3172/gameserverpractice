#include "Player.h"
#include "World.h"
#include <iostream>

Player::Player(asio::ip::tcp::socket socket, int id)
	: GameObject(id), m_socket(std::move(socket))
{
	m_state = ST_ALLOC;
}

void Player::start()
{
	do_recv();
}

void Player::close()
{
	std::lock_guard<std::mutex> lock(m_send_lock);
	asio::error_code ec;
	m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
	m_socket.close(ec);
}

void Player::do_recv()
{
	auto self = shared_from_this();
	m_socket.async_read_some(
		asio::buffer(m_recv_buff + m_prev_remain, sizeof(m_recv_buff) - m_prev_remain),
		[self](asio::error_code ec, size_t bytes_read) {
			if (ec || bytes_read == 0) {
				// 클라이언트가 창을 닫는 정상 종료(eof / connection_reset)는 로그를 남기지 않는다
				if (ec && ec != asio::error::operation_aborted && ec != asio::error::eof
					&& ec != asio::error::connection_reset && ec != asio::error::connection_aborted)
					std::cout << "Receive Error on Session[" << self->m_id << "] EC[" << ec.message() << "]\n";
				World::instance().disconnect(self->m_id);
				return;
			}
			if (!self->process_data(bytes_read)) {
				World::instance().disconnect(self->m_id);
				return;
			}
			self->do_recv();
		});
}

bool Player::process_data(size_t num_bytes)
{
	size_t remain = num_bytes + m_prev_remain;
	unsigned char* p = m_recv_buff;
	while (remain > 0) {
		size_t packet_size = p[0];
		if (packet_size < 2) return false;     // 잘못된 패킷
		if (packet_size > remain) break;
		if (!process_packet(p)) return false;
		p += packet_size;
		remain -= packet_size;
	}
	m_prev_remain = remain;
	if (remain > 0)
		memmove(m_recv_buff, p, remain);
	return true;
}

bool Player::process_packet(unsigned char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN:
		if (packet[0] < sizeof(CS_LOGIN_PACKET)) return false;
		on_login(*reinterpret_cast<CS_LOGIN_PACKET*>(packet));
		return true;
	case CS_MOVE:
		if (packet[0] < sizeof(CS_MOVE_PACKET)) return false;
		on_move(*reinterpret_cast<CS_MOVE_PACKET*>(packet));
		return true;
	default:
		std::cout << "Unknown packet type [" << int(packet[1]) << "] from player[" << m_id << "]\n";
		return true;
	}
}

void Player::on_login(const CS_LOGIN_PACKET& p)
{
	if (m_state != ST_ALLOC) return;
	memcpy(m_name, p.name, NAME_SIZE);
	m_name[NAME_SIZE - 1] = 0;
	m_x = static_cast<short>(rand() % W_WIDTH);
	m_y = static_cast<short>(rand() % W_HEIGHT);

	World::instance().enter_world(shared_from_this());
}

void Player::on_move(const CS_MOVE_PACKET& p)
{
	if (m_state != ST_INGAME) return;
	m_last_move_time = p.move_time;

	short old_x = m_x, old_y = m_y;
	switch (p.direction) {
	case 0: if (m_y > 0) m_y--; break;
	case 1: if (m_y < W_HEIGHT - 1) m_y++; break;
	case 2: if (m_x > 0) m_x--; break;
	case 3: if (m_x < W_WIDTH - 1) m_x++; break;
	default: return;
	}

	auto& world = World::instance();
	world.sectors().update_object(m_id, old_x, old_y, m_x, m_y);
	world.update_view(shared_from_this());
}

void Player::do_send(const void* packet)
{
	size_t size = *reinterpret_cast<const unsigned char*>(packet);
	std::lock_guard<std::mutex> lock(m_send_lock);
	if (!m_socket.is_open()) return;
	asio::error_code ec;
	asio::write(m_socket, asio::buffer(packet, size), ec);
}

void Player::send_login_info()
{
	SC_LOGIN_INFO_PACKET p{};
	p.size = sizeof(p);
	p.type = SC_LOGIN_INFO;
	p.id = m_id;
	p.hp = p.max_hp = 100;
	p.exp = 0;
	p.level = 1;
	p.x = m_x;
	p.y = m_y;
	do_send(&p);
}

void Player::send_add_object(int obj_id)
{
	auto obj = World::instance().get_object(obj_id);
	if (nullptr == obj || obj->m_state != ST_INGAME) return;
	if (!add_view(obj_id)) return;      // 이미 보이는 중

	SC_ADD_OBJECT_PACKET p{};
	p.size = sizeof(p);
	p.type = SC_ADD_OBJECT;
	p.id = obj_id;
	p.x = obj->m_x;
	p.y = obj->m_y;
	strcpy_s(p.name, obj->m_name);
	do_send(&p);
}

void Player::send_move_object(int obj_id)
{
	auto obj = World::instance().get_object(obj_id);
	if (nullptr == obj) return;

	SC_MOVE_OBJECT_PACKET p{};
	p.size = sizeof(p);
	p.type = SC_MOVE_OBJECT;
	p.id = obj_id;
	p.x = obj->m_x;
	p.y = obj->m_y;
	p.move_time = obj->m_last_move_time;
	do_send(&p);
}

void Player::send_remove_object(int obj_id)
{
	if (!remove_view(obj_id)) return;

	SC_REMOVE_OBJECT_PACKET p{};
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	p.id = obj_id;
	do_send(&p);
}

void Player::send_chat(int from_id, const char* mess)
{
	SC_CHAT_PACKET p{};
	p.size = sizeof(p);
	p.type = SC_CHAT;
	p.id = from_id;
	strncpy_s(p.mess, mess, _TRUNCATE);
	do_send(&p);
}
