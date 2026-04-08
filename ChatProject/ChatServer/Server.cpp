#include "pch.h"
#include "SESSION.h"

std::unordered_map<long long, SESSION> clients;

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData); // 버전은 2, 2버전을 사용, 받을 준비 스타트
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // AF_INET - IPv4,
	// SOCK_STREAM - 양방향 통신. 데이터를 오류없이 다른 컴퓨터에 정확하게 도달 손상 및 분실되면 다시 보내기
	// WSA_FLAG_OVERLAPPED - 겹치게 여러개를 오버랩해서 보낼 수 있음
	// 소켓 설정 준비

	//서버 세팅
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; // IP 방식
	server_addr.sin_port = htons(PORT_NUM); // 포트 번호 할당
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY; // 모든 네트워크 IP 수신
	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)); // 소켓에 서버 정보를 할당하여 연결지점 정의
	listen(server, SOMAXCONN); // 받을 준비 완료. 얼마만큼? - 동시 수용 가능한 해당 소켓의 최대 연결 대기 큐 크기만큼
	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); // 빈 iocp 핸들 생성
	CreateIoCompletionPort((HANDLE)server, h_iocp, 0, 0); // 해당 소켓을 iocp에 등록

	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // 클라이언트 접속을 수락하는 소켓 생성

	EXP_OVER accept_over(IO_ACCEPT); // IO타입을 클라이언트 수락용으로 생성
	AcceptEx(server, client_socket, &accept_over.m_buff, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &accept_over.m_over); // 비동기로 클라이언트의 접속을 수락한다.

	for (int i = 1;;)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		LPOVERLAPPED over;
		GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		// iocp 소켓들중 받아올 때까지 대기하다 완료된 것을 받아옴
		if (over == nullptr) {
			error_display(L"GQCS Errror: ", WSAGetLastError());
			continue;
		}
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);

		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT: // 클라이언트 접속 수락
		{
			std::cout << "Client Connected." << std::endl;
			CreateIoCompletionPort((HANDLE)client_socket, h_iocp, i, 0);  // 해당 소켓을 iocp에 등록
			clients.try_emplace(i, i, client_socket); // id와 소켓으로 새 세션 생성 후 등록
			clients[i].do_recv();
			++i;
			client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			AcceptEx(server, client_socket, &accept_over.m_buff, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &accept_over.m_over); // 비동기로 클라이언트의 접속을 수락한다.
		}
		break;
		case IO_RECV: // 수신
		{
			std::cout << "Received message." << std::endl;
			int client_id = static_cast<int>(key); // 키값으로 클라이언트 id 받기
			std::cout << "Client[" << client_id << "] sent: " << clients[client_id].recv_over.m_buff << std::endl; // 현재 받은 정보
			for (auto& cl : clients) cl.second.do_send(client_id, num_bytes, clients[client_id].recv_over.m_buff); // 자신의 세션에 모두 브로드캐스트
			clients[client_id].do_recv(); // 다음 수신 준비
		}
		break;
		case IO_SEND: // 송신
		{
			std::cout << "Message sent." << std::endl;
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over); // over를 EXP_OVER로 캐스팅
			delete o;// 송신 완료 후 동적 할당된 메모리 해제
		}
		break;
		default:
			std::cout << "Unknown IO type." << std::endl;
			break;
		}
	}

	closesocket(server);
	WSACleanup();
}