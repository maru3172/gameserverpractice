#include "pch.h"

std::atomic<int> player_index = 1;
concurrency::concurrent_priority_queue<event_type> timer_queue;
HANDLE g_iocp = nullptr;
SOCKET g_server = INVALID_SOCKET;
tbb::concurrent_unordered_map<int, std::atomic<std::shared_ptr<SESSION>>> clients;

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
