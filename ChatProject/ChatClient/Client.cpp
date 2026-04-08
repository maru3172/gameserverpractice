#include "pch.h"
#include "PACKET.h"
#include "SendRecv.h"

char g_recv_buffer[4096];
char g_send_buffer[4096];
WSABUF g_recv_wsa_buf{ 4096, g_recv_buffer };
WSABUF g_send_wsa_buf{ 4096, g_send_buffer };
WSAOVERLAPPED g_recv_overlapped{}, g_send_overlapped{};
SOCKET g_s_socket;
int g_my_id = -1;
std::vector<sf::String> g_chat_log;

int main()
{
    // 여기서부터
    sf::RenderWindow window(sf::VideoMode(1180, 600), "CHAT WINDOW", sf::Style::Default);
    sf::Font font;
    if (!font.loadFromFile("cour.ttf"))
        return EXIT_FAILURE;

    sf::Event event;
    sf::String playerInput;
    sf::Text playerText("", font, 20);
    playerText.setPosition(60, 560);
    playerText.setFillColor(sf::Color::Yellow);

    // 입력창 구분선
    sf::RectangleShape divider(sf::Vector2f(1160, 2));
    divider.setPosition(10, 545);
    divider.setFillColor(sf::Color::White);

    // 입력 프롬프트
    sf::Text promptText("> ", font, 20);
    promptText.setPosition(10, 560);
    promptText.setFillColor(sf::Color::White);
    // 여기까진 전부 클라이언트에 그리기 용도

    std::wcout.imbue(std::locale("korean"));
    WSADATA wsa_data{};
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    g_s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    SOCKADDR_IN server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    int result = WSAConnect(g_s_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr), nullptr, nullptr, nullptr, nullptr);
    if (result == SOCKET_ERROR) {
        error_display(L"서버 연결 실패", WSAGetLastError());
        return 1;
    }

    recv_from_server(); // 수신 스타트

    while (window.isOpen())
    {
        window.clear();
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) { // 윈도우 닫히면 꺼지는거..
                window.close();
                break;
            }

            if (event.type == sf::Event::TextEntered) // 텍스트를 치면
            {
                if (event.text.unicode < 128)
                {
                    if (13 == event.text.unicode) // 엔터가 쳐지면..
                    {
                        // 입력한 내용을 문자열로 저장, 입력창 비우기
                        std::string message = playerInput.toAnsiString();
                        playerInput.clear();
                        
                        // 보낼려는 채팅이 빈 메세지가 아니라면
                        if (!message.empty())
                        {
                            PACKET pkt(g_my_id, message.c_str()); // 패킷에 id와 메세지 등록
                            g_send_wsa_buf.len = pkt.m_size; // 전송 버퍼에 패킷 크기 설정
                            memcpy(g_send_wsa_buf.buf, &pkt, pkt.m_size); // 버퍼에 복사
                            ZeroMemory(&g_send_overlapped, sizeof(g_send_overlapped)); // 잔여 데이터 제거
                            DWORD sent_size = 0;
                            int result = WSASend(g_s_socket, &g_send_wsa_buf, 1, &sent_size, 0, &g_send_overlapped, send_callback);
                            // 전송 스타트
                            
                            // 데이터 전송 실패하면 에러!
                            if (result == SOCKET_ERROR) {
                                int err_no = WSAGetLastError();
                                if (err_no != WSA_IO_PENDING) {
                                    error_display(L"데이터 전송 실패", err_no);
                                    exit(1);
                                }
                            }
                        }
                    }
                    else if (8 == event.text.unicode) // 백스페이스, 글자 하나씩 지움
                    {
                        if (playerInput.getSize() > 0) playerInput.erase(playerInput.getSize() - 1);
                    }

                    // 입력한 문자를 입력창에 추가 후 화면 업데이트
                    else playerInput += event.text.unicode;
                    playerText.setString(playerInput);
                }
            }
        }

        // 채팅 로그 출력
        for (size_t i = 0; i < g_chat_log.size(); ++i) {
            sf::Text chatLine(g_chat_log[i], font, 18);
            chatLine.setPosition(10, 10 + static_cast<float>(i) * 25);
            chatLine.setFillColor(sf::Color::White);
            window.draw(chatLine);
        }

        window.draw(divider);
        window.draw(promptText);
        window.draw(playerText);
        window.display();
        SleepEx(0, TRUE);
    }

    closesocket(g_s_socket);
    WSACleanup();
}