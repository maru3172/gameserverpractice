// protobuf 헤더는 windows.h / using namespace std 보다 먼저 포함한다 (std::byte 충돌 방지)
#include "protocol.pb.h"
#include "PacketDefs.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <array>
#include <memory>

#include "../include/NetworkModule.h"

#pragma comment (lib, "ws2_32.lib")

using namespace std::chrono;

extern HWND hWnd;

constexpr int MAX_TEST = 10000;
constexpr int MAX_CLIENTS = MAX_TEST * 2;
constexpr int INVALID_ID = -1;
constexpr int MAX_BUFF_SIZE = 255;
constexpr int SEND_BUFFER = 256;
constexpr int NUM_WORKERS = 6;

HANDLE g_hiocp;

enum OPTYPE { OP_SEND, OP_RECV };

struct OverlappedEx {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	unsigned char IOCP_buf[MAX_BUFF_SIZE];
	OPTYPE event_type;
};

struct CLIENT {
	int id;
	std::atomic<int> x;
	std::atomic<int> y;
	std::atomic_bool connected;

	SOCKET client_socket;
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
	high_resolution_clock::time_point last_move_time;
};

// 서버 ID -> 테스트 클라이언트 인덱스
std::array<std::atomic<int>, NPC_ID_START> client_map;
std::array<CLIENT, MAX_CLIENTS> g_clients;
std::atomic_int num_connections;
std::atomic_int client_to_close;
std::atomic_int active_clients;

int global_delay;    // ms 단위, 1000 이 넘으면 클라이언트 증가 종료

std::vector<std::thread> worker_threads;
std::thread test_thread;

float point_cloud[MAX_CLIENTS * 2];

high_resolution_clock::time_point last_connect_time;

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);
	std::cout << msg;
	std::wcout << L" [" << err_no << L"] " << (lpMsgBuf ? lpMsgBuf : L"") << std::endl;
	LocalFree(lpMsgBuf);
}

void DisconnectClient(int ci)
{
	bool status = true;
	if (g_clients[ci].connected.compare_exchange_strong(status, false)) {
		closesocket(g_clients[ci].client_socket);
		active_clients--;
	}
}

void SendPacket(int cl, const google::protobuf::MessageLite& msg, unsigned char packet_type)
{
	size_t body = msg.ByteSizeLong();
	size_t total = body + PACKET_HEADER_SIZE;
	if (total > MAX_BUFF_SIZE) return;

	OverlappedEx* over = new OverlappedEx;
	ZeroMemory(&over->over, sizeof(over->over));
	over->event_type = OP_SEND;
	over->IOCP_buf[0] = static_cast<unsigned char>(total);
	over->IOCP_buf[1] = packet_type;
	msg.SerializeToArray(&over->IOCP_buf[PACKET_HEADER_SIZE], static_cast<int>(body));

	over->wsabuf.buf = reinterpret_cast<CHAR*>(over->IOCP_buf);
	over->wsabuf.len = static_cast<ULONG>(total);
	int ret = WSASend(g_clients[cl].client_socket, &over->wsabuf, 1, NULL, 0, &over->over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			delete over;
			DisconnectClient(cl);
		}
	}
}

void ProcessPacket(int ci, const unsigned char* pb, unsigned char pb_type, int pb_size)
{
	switch (pb_type) {
	case PKID_SC_POS: {
		SC_POS p;
		p.ParseFromArray(pb, pb_size);
		if (p.id() < 0 || p.id() >= NPC_ID_START) break;
		int my_id = client_map[p.id()];
		if (my_id < 0 || my_id >= MAX_CLIENTS || g_clients[my_id].id != p.id()) break;
		g_clients[my_id].x = p.x();
		g_clients[my_id].y = p.y();
		if (ci == my_id && 0 != p.move_time()) {
			long long now_ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
			long long d_ms = now_ms - p.move_time();
			if (global_delay < d_ms) global_delay++;
			else if (global_delay > d_ms) global_delay--;
		}
		break;
	}
	case PKID_SC_PUT_OBJECT: break;
	case PKID_SC_REMOVE_OBJECT: break;
	case PKID_SC_LOGIN_OK: {
		SC_LOGIN_OK p;
		p.ParseFromArray(pb, pb_size);
		if (p.id() < 0 || p.id() >= NPC_ID_START) break;
		client_map[p.id()] = ci;
		g_clients[ci].id = p.id();
		g_clients[ci].x = p.x();
		g_clients[ci].y = p.y();
		bool expected = false;
		if (g_clients[ci].connected.compare_exchange_strong(expected, true))
			active_clients++;
		break;
	}
	default:
		std::cout << "Unknown Packet Type " << int(pb_type) << "\n";
		break;
	}
}

void Worker_Thread()
{
	while (true) {
		DWORD io_size = 0;
		ULONG_PTR ci = 0;
		OverlappedEx* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &ci,
			reinterpret_cast<LPWSAOVERLAPPED*>(&over), INFINITE);
		int client_id = static_cast<int>(ci);

		if (nullptr == over) continue;

		if (OP_SEND == over->event_type) {
			if (FALSE == ret || io_size != over->wsabuf.len)
				DisconnectClient(client_id);
			delete over;
			continue;
		}

		// OP_RECV
		if (FALSE == ret || 0 == io_size) {
			DisconnectClient(client_id);
			continue;
		}

		CLIENT& cl = g_clients[client_id];
		unsigned char* buf = cl.recv_over.IOCP_buf;
		unsigned psize = cl.curr_packet_size;
		unsigned pr_size = cl.prev_packet_data;
		bool bad_packet = false;
		while (io_size > 0) {
			if (0 == psize) psize = (pr_size > 0) ? cl.packet_buf[0] : buf[0];
			if (psize < PACKET_HEADER_SIZE) { bad_packet = true; break; }
			if (io_size + pr_size >= psize) {
				unsigned char packet[MAX_PACKET_SIZE];
				memcpy(packet, cl.packet_buf, pr_size);
				memcpy(packet + pr_size, buf, psize - pr_size);
				ProcessPacket(client_id, &packet[PACKET_HEADER_SIZE], packet[1], psize - PACKET_HEADER_SIZE);
				io_size -= psize - pr_size;
				buf += psize - pr_size;
				psize = 0;
				pr_size = 0;
			}
			else {
				memcpy(cl.packet_buf + pr_size, buf, io_size);
				pr_size += io_size;
				io_size = 0;
			}
		}
		if (bad_packet) {
			DisconnectClient(client_id);
			continue;
		}
		cl.curr_packet_size = psize;
		cl.prev_packet_data = pr_size;

		DWORD recv_flag = 0;
		ZeroMemory(&cl.recv_over.over, sizeof(cl.recv_over.over));
		int r = WSARecv(cl.client_socket, &cl.recv_over.wsabuf, 1, NULL, &recv_flag, &cl.recv_over.over, NULL);
		if (SOCKET_ERROR == r && WSAGetLastError() != WSA_IO_PENDING)
			DisconnectClient(client_id);
	}
}

constexpr int DELAY_LIMIT = 100;
constexpr int DELAY_LIMIT2 = 150;
constexpr int ACCEPT_DELY = 50;

void Adjust_Number_Of_Client()
{
	static int delay_multiplier = 1;
	static int max_limit = MAX_TEST;
	static bool increasing = true;

	if (active_clients >= MAX_TEST) return;
	if (num_connections >= MAX_CLIENTS) return;

	auto duration = high_resolution_clock::now() - last_connect_time;
	if (ACCEPT_DELY * delay_multiplier > duration_cast<milliseconds>(duration).count()) return;

	int t_delay = global_delay;
	if (DELAY_LIMIT2 < t_delay) {
		if (true == increasing) {
			max_limit = active_clients;
			increasing = false;
		}
		if (100 > active_clients) return;
		if (ACCEPT_DELY * 10 > duration_cast<milliseconds>(duration).count()) return;
		last_connect_time = high_resolution_clock::now();
		DisconnectClient(client_to_close);
		client_to_close++;
		return;
	}
	else if (DELAY_LIMIT < t_delay) {
		delay_multiplier = 10;
		return;
	}
	if (max_limit - (max_limit / 20) < active_clients) return;

	increasing = true;
	last_connect_time = high_resolution_clock::now();

	int ci = num_connections;
	CLIENT& cl = g_clients[ci];
	cl.client_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	int result = WSAConnect(cl.client_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr),
		NULL, NULL, NULL, NULL);
	if (0 != result) {
		error_display("WSAConnect : ", WSAGetLastError());
		closesocket(cl.client_socket);
		return;     // 서버가 꺼져 있으면 다음 주기에 다시 시도
	}

	BOOL no_delay = TRUE;
	setsockopt(cl.client_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&no_delay), sizeof(no_delay));

	cl.curr_packet_size = 0;
	cl.prev_packet_data = 0;
	ZeroMemory(&cl.recv_over, sizeof(cl.recv_over));
	cl.recv_over.event_type = OP_RECV;
	cl.recv_over.wsabuf.buf = reinterpret_cast<CHAR*>(cl.recv_over.IOCP_buf);
	cl.recv_over.wsabuf.len = sizeof(cl.recv_over.IOCP_buf);
	cl.last_move_time = high_resolution_clock::now();

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(cl.client_socket), g_hiocp, ci, 0);

	char user_name[MAX_ID_LEN];
	sprintf_s(user_name, "P%d", ci);
	CS_LOGIN packet;
	packet.set_user_name(user_name);
	SendPacket(ci, packet, PKID_CS_LOGIN);

	DWORD recv_flag = 0;
	int ret = WSARecv(cl.client_socket, &cl.recv_over.wsabuf, 1, NULL, &recv_flag, &cl.recv_over.over, NULL);
	if (SOCKET_ERROR == ret && WSAGetLastError() != WSA_IO_PENDING)
		error_display("RECV ERROR", WSAGetLastError());

	num_connections++;
}

void Test_Thread()
{
	while (true) {
		Adjust_Number_Of_Client();

		int count = num_connections;
		for (int i = 0; i < count; ++i) {
			if (false == g_clients[i].connected) continue;
			if (g_clients[i].last_move_time + 1s > high_resolution_clock::now()) continue;
			g_clients[i].last_move_time = high_resolution_clock::now();

			CS_MOVE packet;
			switch (rand() % 4) {
			case 0: packet.set_dir(Direction::UP); break;
			case 1: packet.set_dir(Direction::DOWN); break;
			case 2: packet.set_dir(Direction::LEFT); break;
			case 3: packet.set_dir(Direction::RIGHT); break;
			}
			packet.set_move_time(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());
			SendPacket(i, packet, PKID_CS_MOVE);
		}
		std::this_thread::sleep_for(1ms);
	}
}

void InitializeNetwork()
{
	for (auto& cl : g_clients) {
		cl.connected = false;
		cl.id = INVALID_ID;
	}
	for (auto& m : client_map) m = INVALID_ID;
	num_connections = 0;
	last_connect_time = high_resolution_clock::now();

	WSADATA wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);

	for (int i = 0; i < NUM_WORKERS; ++i)
		worker_threads.emplace_back(Worker_Thread);

	test_thread = std::thread{ Test_Thread };
}

void ShutdownNetwork()
{
	test_thread.join();
	for (auto& th : worker_threads) th.join();
}

void Do_Network()
{
}

void GetPointCloud(int* size, float** points)
{
	int index = 0;
	int count = num_connections;
	for (int i = 0; i < count; ++i) {
		if (true == g_clients[i].connected) {
			point_cloud[index * 2] = static_cast<float>(g_clients[i].x);
			point_cloud[index * 2 + 1] = static_cast<float>(g_clients[i].y);
			index++;
		}
	}
	*size = index;
	*points = point_cloud;
}
