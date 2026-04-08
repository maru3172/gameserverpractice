#include "pch.h"
#include "EXP_OVER.h"

EXP_OVER::EXP_OVER()
{
	ZeroMemory(&m_over, sizeof(m_over));
	m_wsa.buf = m_buff;
	m_wsa.len = BUF_SIZE;
}

EXP_OVER::EXP_OVER(IOType iot) : m_iotype(iot)
{
	ZeroMemory(&m_over, sizeof(m_over));
	m_wsa.buf = m_buff;
	m_wsa.len = BUF_SIZE;
}
