#include "pch.h"

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
