#pragma once

class OVER_EXP
{
public:
	OVER_EXP();

	OVER_EXP(char* packet);

	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	int _ai_target_obj;
};

