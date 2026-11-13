#include "World.h"
#include <iostream>
#include <thread>

using namespace std::chrono;

World& World::instance()
{
	static World world;
	return world;
}

void World::initialize(asio::io_context& io, unsigned short port)
{
	m_io = &io;
	m_players = std::make_unique<std::atomic<std::shared_ptr<Player>>[]>(MAX_USER);
	for (int i = 0; i < MAX_USER; ++i)
		m_free_ids.push(i);

	initialize_npcs();

	m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
	do_accept();
	std::cout << "Server listening on port " << port << ".\n";
}

void World::initialize_npcs()
{
	std::cout << "NPC initialize begin.\n";
	m_npcs.reserve(MAX_NPC);
	for (int i = 0; i < MAX_NPC; ++i) {
		auto npc = std::make_unique<Npc>(MAX_USER + i);
		m_sectors.add_object(npc->id(), npc->m_x, npc->m_y);
		m_npcs.push_back(std::move(npc));
	}
	std::cout << "NPC initialize end. (" << MAX_NPC << " NPCs)\n";
}

std::shared_ptr<Player> World::get_player(int id)
{
	if (id < 0 || id >= MAX_USER) return nullptr;
	return m_players[id].load();
}

Npc* World::get_npc(int id)
{
	int index = id - MAX_USER;
	if (index < 0 || index >= static_cast<int>(m_npcs.size())) return nullptr;
	return m_npcs[index].get();
}

std::shared_ptr<GameObject> World::get_object(int id)
{
	if (is_pc(id)) return get_player(id);
	Npc* npc = get_npc(id);
	if (nullptr == npc) return nullptr;
	// NPC 는 서버 종료까지 살아 있으므로 소유권 없는 shared_ptr 로 넘긴다.
	return std::shared_ptr<GameObject>(std::shared_ptr<GameObject>{}, npc);
}

void World::do_accept()
{
	m_acceptor->async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
		if (!ec) {
			int new_id;
			if (!m_free_ids.try_pop(new_id)) {
				std::cout << "Server is full. Connection rejected.\n";
				asio::error_code ignore;
				socket.close(ignore);
			}
			else {
				socket.set_option(asio::ip::tcp::no_delay(true));
				auto player = std::make_shared<Player>(std::move(socket), new_id);
				m_players[new_id].store(player);
				player->start();
			}
		}
		do_accept();
		});
}

void World::enter_world(const std::shared_ptr<Player>& player)
{
	S_STATE expected = ST_ALLOC;
	if (!player->m_state.compare_exchange_strong(expected, ST_INGAME)) return;

	m_sectors.add_object(player->id(), player->m_x, player->m_y);
	player->send_login_info();
	update_view(player);
}

void World::update_view(const std::shared_ptr<Player>& player)
{
	const int my_id = player->id();
	std::unordered_set<int> old_view = player->copy_view();

	std::unordered_set<int> new_view;
	for (int id : m_sectors.get_objects_in_adjacent_sectors(player->m_x, player->m_y)) {
		if (id == my_id) continue;
		auto obj = get_object(id);
		if (!obj || obj->m_state != ST_INGAME) continue;
		if (player->can_see(*obj)) new_view.insert(id);
	}

	player->send_move_object(my_id);

	for (int id : new_view) {
		if (is_pc(id)) {
			auto other = get_player(id);
			if (!other) continue;
			if (old_view.count(id) == 0) player->send_add_object(id);
			if (other->in_view(my_id)) other->send_move_object(my_id);
			else other->send_add_object(my_id);
		}
		else {
			player->send_add_object(id);   // 이미 보이면 내부에서 무시
			if (Npc* npc = get_npc(id)) npc->wake_up(my_id);
		}
	}

	for (int id : old_view) {
		if (new_view.count(id)) continue;
		player->send_remove_object(id);
		if (is_pc(id))
			if (auto other = get_player(id)) other->send_remove_object(my_id);
	}
}

void World::disconnect(int player_id)
{
	auto player = get_player(player_id);
	if (!player) return;

	S_STATE old_state = player->m_state.exchange(ST_FREE);
	if (old_state == ST_FREE) return;

	if (old_state == ST_INGAME)
		m_sectors.remove_object(player_id, player->m_x, player->m_y);

	for (int id : player->take_view()) {
		if (!is_pc(id)) continue;
		if (auto other = get_player(id)) other->send_remove_object(player_id);
	}

	player->close();
	m_players[player_id].store(nullptr);
	m_free_ids.push(player_id);
}

void World::add_timer(int obj_id, milliseconds delay)
{
	m_timer_queue.push(TimerEvent{ obj_id, system_clock::now() + delay });
}

void World::timer_loop()
{
	for (;;) {
		TimerEvent ev;
		if (!m_timer_queue.try_pop(ev)) {
			std::this_thread::sleep_for(1ms);
			continue;
		}
		if (ev.wakeup_time > system_clock::now()) {
			m_timer_queue.push(ev);
			std::this_thread::sleep_for(1ms);
			continue;
		}
		int obj_id = ev.obj_id;
		asio::post(*m_io, [obj_id]() {
			if (Npc* npc = World::instance().get_npc(obj_id))
				npc->on_timer();
			});
	}
}

void World::run()
{
	std::vector<std::thread> workers;
	unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
	for (unsigned int i = 0; i < num_threads; ++i)
		workers.emplace_back([this] { m_io->run(); });

	std::thread timer_thread([this] { timer_loop(); });

	timer_thread.join();
	for (auto& th : workers) th.join();
}
