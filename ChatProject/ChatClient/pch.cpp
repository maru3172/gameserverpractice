#include "pch.h"

void add_chat_line(const sf::String& line)
{
    g_chat_log.push_back(line); // 메세지 저장
    if (g_chat_log.size() > MAX_CHAT_LINES) // 채팅 오래되면 가장 오래된거부터 제거
        g_chat_log.erase(g_chat_log.begin());
}

void error_display(const wchar_t* msg, int err_no)
{
    WCHAR* lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, err_no,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    std::wcout << msg;
    std::wcout << L" === 에러 " << lpMsgBuf << std::endl;
    while (true);
    LocalFree(lpMsgBuf);
}
