#include "pch.h"
#include "SectorManager.h"

std::atomic<int> player_index = 1;
concurrency::concurrent_priority_queue<event_type> timer_queue;

SectorManager sector_manager;
HANDLE g_iocp;
SOCKET g_server;

void error_display(const wchar_t* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::wcout << msg;
	std::wcout << L" === 에러 " << lpMsgBuf << std::endl;
	// while (true);   // 디버깅 용
	LocalFree(lpMsgBuf);
}

bool is_pc(int id)
{
	return id < NPC_ID_START;
}

bool is_npc(int id)
{
	return id >= NPC_ID_START;
}
