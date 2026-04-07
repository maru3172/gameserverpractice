#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include <MSWSock.h>
#include "EXP_OVER.h"
#include "SESSION.h"

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")
constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;

enum IO_Type { IO_SEND, IO_RECV, IO_ACCEPT };

void error_display(const wchar_t* msg, int err_no)
{
	WCHAR* IpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&IpMsgBuf, 0, NULL);
	std::wcout << msg;
	std::wcout << L" == 에러 발생: " << IpMsgBuf << std::endl;
	while (true); // 로그 띄우고 확인
	LocalFree(IpMsgBuf); // 할당된 메모리 해제
}