#include "pch.h"
#include "PACKET.h"
#include "SendRecv.h"

void recv_from_server() // 서버로부터 수신
{
    DWORD recv_flag = 0;
    ZeroMemory(&g_recv_overlapped, sizeof(g_recv_overlapped));
    int result = WSARecv(g_s_socket, &g_recv_wsa_buf, 1, nullptr, &recv_flag, &g_recv_overlapped, recv_callback);

    // 데이터 받지 못했다면 에러 로그 등록
    if (result == SOCKET_ERROR) {
        int err_no = WSAGetLastError();
        if (err_no != WSA_IO_PENDING) {
            error_display(L"데이터 수신 실패", err_no);
            exit(1);
        }
    }
}

void CALLBACK recv_callback(DWORD error, DWORD bytes_transferred, LPWSAOVERLAPPED overlapped, DWORD flags)
{
    // 수신 못받으면 에러!
    if (error != 0) {
        error_display(L"데이터 수신 실패", WSAGetLastError());
        exit(1);
    }

    int remain = static_cast<int>(bytes_transferred); // 수신된 총 바이트 수
    char* ptr = g_recv_buffer; // 수신 버퍼 시작 위치

    while (remain >= 2) // 앞 2바이트는 size와 id를 기록하기에 나머지 데이터를 처리해야됨
    {
        unsigned char pkt_size = static_cast<unsigned char>(*ptr); // 그 위치에서 패킷 크기 받아옴
        if (pkt_size == 0 || pkt_size > remain) break; // 패킷이 크기가 없거나 패킷 사이즈보다 현재 받은 바이트가 적을 땐 나감(다 못 받은 것)

        // 패킷을 받고 데이터를 출력하는 부분
        PACKET* packet = reinterpret_cast<PACKET*>(ptr);
        int id = packet->m_sender_id; // 송신자 ID 추출

        std::string token(packet->m_buf);  // 패킷 데이터를 문자열로 변환
        token.erase(0, 2); // 앞 2바이트는 size와 id가 저장되어 있음, 제거해야만 정상 출력
        std::string msg = "Client[" + std::to_string(id) + "]: " + (token);
        std::cout << msg << std::endl;
        add_chat_line(sf::String(msg)); // 채팅 로그 등록

        ptr += pkt_size; // 현재 채팅에서 다음 처리할 패킷 확인
        remain -= pkt_size; // 처리하고 남은 수신 데이터(만약 남으면 추가로 작업)
    }

    recv_from_server(); // 다시 받을 준비
}

void CALLBACK send_callback(DWORD error, DWORD bytes_transferred, LPWSAOVERLAPPED overlapped, DWORD flags)
{
    // 전송 실패하면 에러!
    if (error != 0) {
        error_display(L"데이터 전송 실패", WSAGetLastError());
        exit(1);
    }

    // 서버로 보낸 데이터 사이즈 로그 기록
    std::cout << "Sent to server: SIZE: " << bytes_transferred << std::endl;
}
