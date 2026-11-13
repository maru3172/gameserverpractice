#include "GameServer.h"
#include <iostream>

int main()
{
	// ODBC 오류 메시지(한글)를 wcout 으로 출력할 수 있도록 시스템 로케일 사용
	std::wcout.imbue(std::locale(""));

	auto& server = GameServer::instance();
	if (!server.initialize()) {
		std::cout << "Server initialize failed.\n";
		return 1;
	}
	server.run();
	return 0;
}
