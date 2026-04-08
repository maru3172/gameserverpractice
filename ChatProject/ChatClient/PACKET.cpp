#include "pch.h"
#include "PACKET.h"

PACKET::PACKET(int sender, const char* mess) : m_sender_id(static_cast<unsigned char>(sender))
{
	strcpy_s(m_buf, mess);
	m_size = static_cast<unsigned char>(strlen(mess) + 1 + 2); // data + null + header(2 size | id)
}
