#include "SESSION.h"

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&recv_over.m_over, 0, sizeof(recv_over.m_over)); // 현재 상태를 전부 초기화, 받을 준비
	recv_over.m_iotype = IO_RECV; // IO타입 - 수신으로 설정
	recv_over.m_wsa.len = BUF_SIZE; // 사이즈는 버퍼 사이즈만큼
	WSARecv(client, &recv_over.m_wsa, 1, 0, &recv_flag, &recv_over.m_over, nullptr); // 클라이언트로부터 비동기 수신 요청
}

void SESSION::do_send(int sender_id, int num_bytes, char* mess)
{
	EXP_OVER* o = new EXP_OVER(IO_SEND); // IO 타입을 송신으로 설정
	o->m_buff[0] = num_bytes + 2; // 보내야 하는 크기
	o->m_buff[1] = sender_id; // 송신자 id 등록
	memcpy(o->m_buff + 2, mess, num_bytes); //  buff[0]=크기, buff[1]=id 이후 위치부터 채팅 내용 복사
	o->m_wsa.len = num_bytes + 2; // 길이를 같이 알려줘야함
	WSASend(client, &o->m_wsa, 1, 0, 0, &o->m_over, nullptr); // 클라이언트에게 비동기 송신 요청
}
