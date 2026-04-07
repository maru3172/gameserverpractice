#include "EXP_OVER.h"

EXP_OVER::EXP_OVER()
{
	ZeroMemory(&m_over, sizeof(m_over)); //오버랩 데이터를 전부 0으로 초기화
	m_wsa.buf = m_buff; // 실제 데이터 버퍼와 포인터를 연결
	m_wsa.len = BUF_SIZE; // 사이즈 알려주기
}

EXP_OVER::EXP_OVER(IO_Type iot) : m_iotype(iot)
{
	ZeroMemory(&m_over, sizeof(m_over));
	m_wsa.buf = m_buff;
	m_wsa.len = BUF_SIZE;
}
