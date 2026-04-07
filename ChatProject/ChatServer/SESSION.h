#pragma once
#include "pch.h"

class SESSION;
std::unordered_map<long long, SESSION> clients; // key=id, value=세션
class SESSION
{
public:
	SESSION() { exit(-1); }
	SESSION(int id, SOCKET so) : m_id(id), client(so) {}

	~SESSION() { closesocket(client); }

	void do_recv();
	void do_send(int sender_id, int num_bytes, char* mess);

	EXP_OVER recv_over; // 수신자 오버랩 정보
private:
	SOCKET client; // 클라이언트 정보
	long long m_id; // id
};

