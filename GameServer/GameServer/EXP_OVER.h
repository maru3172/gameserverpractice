#pragma once

class EXP_OVER
{
public:
	EXP_OVER();

	EXP_OVER(IOType iot);

	WSAOVERLAPPED m_over;
	IOType  m_iotype;
	WSABUF	m_wsa;
	char  m_buff[BUF_SIZE];
};

