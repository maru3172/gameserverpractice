#pragma once

// 비동기 io 작업을 위한 구조체, 작업의 상태와 완료를 담는 랩
// 그러나 여기서는 overlapped 구조체를 확장
class EXP_OVER
{
public:
	EXP_OVER();

	EXP_OVER(IO_Type iot);

	WSAOVERLAPPED m_over; // OS가 비동기 IO 작업을 추적하고 상태를 확인하는 구조체
	IO_Type m_iotype; // 현재 IO 타입
	WSABUF m_wsa; // 실제 데이터의 위치(포인터)와 길이를 저장
	char m_buff[BUF_SIZE]; // 실제 버퍼
};
