#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>
#include "Common.h"

namespace google::protobuf { class MessageLite; }

// 접속한 클라이언트 하나. 수신 버퍼 재조립, 시야 목록, protobuf 패킷 송신을 담당한다.
class Session {
public:
	Session(SOCKET socket, int id);
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	int id() const { return m_id; }
	bool start_recv();
	void close_socket();

	// 수신 완료 처리: 완성된 패킷마다 handler(type, payload, payload_size) 호출. 실패 시 false.
	template <typename Handler>
	bool on_recv(DWORD num_bytes, Handler&& handler);

	bool can_see(const Session& other) const;

	void send_login_ok();
	void send_put_object(const Session& obj);
	void send_pos(const Session& mover);
	void send_remove_object(int obj_id);

	bool in_view(int id);
	std::unordered_set<int> copy_view();
	std::unordered_set<int> take_view();

public:
	std::atomic<SESSION_STATE> m_state{ ST_CONNECTED };
	short m_x, m_y;
	long long m_move_time = 0;
	std::string m_name;
	OVER_EX m_recv_over{ EV_RECV };

private:
	void send_packet(unsigned char type, const google::protobuf::MessageLite& msg);

	SOCKET m_socket;
	const int m_id;
	unsigned char m_packet_buf[MAX_BUFFER];
	int m_saved = 0;

	std::unordered_set<int> m_view;
	std::mutex m_view_lock;
};

template <typename Handler>
bool Session::on_recv(DWORD num_bytes, Handler&& handler)
{
	// 받은 데이터를 m_packet_buf 뒤에 이어 붙이고 완성된 패킷부터 처리한다.
	const unsigned char* src = reinterpret_cast<const unsigned char*>(m_recv_over.net_buf);
	size_t remain = num_bytes;
	while (remain > 0) {
		size_t space = sizeof(m_packet_buf) - m_saved;
		size_t copy = remain < space ? remain : space;
		memcpy(m_packet_buf + m_saved, src, copy);
		m_saved += static_cast<int>(copy);
		src += copy;
		remain -= copy;

		int pos = 0;
		while (m_saved - pos >= 1) {
			int packet_size = m_packet_buf[pos];
			if (packet_size < PACKET_HEADER_SIZE) return false;
			if (m_saved - pos < packet_size) break;
			if (!handler(m_packet_buf[pos + 1], m_packet_buf + pos + PACKET_HEADER_SIZE,
				packet_size - PACKET_HEADER_SIZE))
				return false;
			pos += packet_size;
		}
		if (pos > 0) {
			memmove(m_packet_buf, m_packet_buf + pos, m_saved - pos);
			m_saved -= pos;
		}
	}
	return true;
}
