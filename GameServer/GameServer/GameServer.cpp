#include "pch.h"
#include "SESSION.h"

void send_login_fail(SOCKET client, const char* message)
{
	// 플레이어 로그인 실패 및 사유
	S2C_LoginResult packet;
	packet.size = sizeof(S2C_LoginResult);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strncpy_s(packet.message, message, sizeof(packet.message));
	WSABUF wsa_buf;
	wsa_buf.buf = reinterpret_cast<char*>(&packet);
	wsa_buf.len = packet.size;
	WSASend(client, &wsa_buf, 1, 0, 0, nullptr, nullptr);
}

int main()
{
	// 서버 초기화, IOCP 생성 및 등록, 클라이언트 연결 대기
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);
	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)server, h_iocp, -1, 0);

	// 클라이언트 소켓 등록
	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	for (int player_index = 0;;)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		LPOVERLAPPED over;
		// IOCP 소켓 중 IO 작업 완료된 것을 받아오고 그렇지 않으면 무한정 대기
		GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		// 연결 끊김 ->
		if (over == nullptr) {
			error_display(L"GQCS Errror: ", WSAGetLastError());
			if (key == -1) { // 소켓 자체에서 문제 발생
				exit(-1);
			}
			// 클라이언트 연결 종료
			std::cout << "client[" << key << "] Disconnected.\n";
			clients[key].m_is_connected = false;
			for (auto& cl : clients)
				if (true == cl.m_is_connected) cl.send_remove_player(key);
			closesocket(clients[key].m_client);
			clients[key].m_client = INVALID_SOCKET;
			continue;
		}
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);
		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			std::cout << "Client connected." << std::endl;
		}
		break;
		case IO_RECV:
		{

		}
		break;
		case IO_SEND: // 송신 완료한 내용 메모리 해제
		{
			std::cout << "Message sent. to client[" << key << "]\n";
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			delete o;
		}
		break;
		default: // 알 수 없는 이상한 IO 타입을 보내옴
		{
			std::cout << "Unknown IO type." << std::endl;
			exit(-1);
		}
		break;
		}
	}

	closesocket(server);
	WSACleanup();
}