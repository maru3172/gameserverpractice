#pragma once

#pragma pack(push, 1) // 컴파일러 메모리 정렬(패딩)제거, 온전히 패킷을 보낼 수 있게 설정
// 송수신 데이터를 패킷 형태로 묶는 클래스 (크기, 송신자 id, 메시지)
class PACKET
{
public:
	PACKET() : m_size(0), m_sender_id(0) { m_buf[0] = '\0'; };
	PACKET(int sender, const char* mess);

	unsigned char m_size;
	unsigned char m_sender_id;
	char m_buf[BUFFER_SIZE];
};
#pragma pack(pop)
