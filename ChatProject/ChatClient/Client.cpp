#include "pch.h"

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

    closesocket(g_s_socket);
    WSACleanup();
}