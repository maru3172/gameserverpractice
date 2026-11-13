#include "World.h"
#include <iostream>

int main()
{
	try {
		asio::io_context io_context;
		auto& world = World::instance();
		world.initialize(io_context, PORT_NUM);
		world.run();
	}
	catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
		return 1;
	}
	return 0;
}
