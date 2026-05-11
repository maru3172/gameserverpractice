#include "pch.h"
#include "NPC_Session.h"

NPC_Session::NPC_Session()
{
	_id = -1;
	_socket = 0;
	x = y = 0;
	_name[0] = 0;
	_state = ST_FREE;
	_prev_remain = 0;
}

void NPC_Session::do_recv()
{
	DWORD recv_flag = 0;
	memset(&_recv_over._over, 0, sizeof(_recv_over._over));
	_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
	_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
	WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over, 0);
}

void NPC_Session::do_send(void* packet)
{
	OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
	WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
}

void NPC_Session::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET p;
	p.id = _id;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.x = x;
	p.y = y;
	do_send(&p);
}

void NPC_Session::send_remove_player_packet(int c_id)
{
	_vl.lock();
	if (_view_list.count(c_id)) _view_list.erase(c_id);
	else {
		_vl.unlock();
		return;
	}
	_vl.unlock();
	SC_REMOVE_OBJECT_PACKET p;
	p.id = c_id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	do_send(&p);
}