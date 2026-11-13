#include "GameServer.h"
#include <iostream>
#include <thread>

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	GameServer server;
	int workers = static_cast<int>(std::thread::hardware_concurrency());
	if (workers < 1) workers = 4;
	if (!server.initialize(SERVER_PORT, workers)) {
		std::cout << "Server initialize failed.\n";
		return 1;
	}
	server.run();

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
