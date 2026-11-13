#include <iostream>
#include <sdkddkver.h>
#include <asio.hpp>
#include <flatbuffers/flatbuffers.h>
#include "protocol_generated.h"

constexpr int PORT = 3500;
constexpr   int BUFSIZE = 1024;

std::string read_message(asio::ip::tcp::socket& s)
{
    char buf[BUFSIZE];

    size_t len = s.read_some(asio::buffer(buf, BUFSIZE));

    // 받은 데이터가 올바른 FlatBuffer 인지 검증 (잘못된 데이터로 인한 크래시 방지)
    flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(buf), len);
    if (!verifier.VerifyBuffer<Message_C2S>(nullptr))
        throw std::runtime_error("invalid flatbuffer message");

    auto packet = flatbuffers::GetRoot<Message_C2S>(buf);
    auto mess = packet->msg();
    return mess ? flatbuffers::GetString(mess) : std::string{};
}

void send_message(asio::ip::tcp::socket& s, std::string &mess)
{
    flatbuffers::FlatBufferBuilder fbb;
    auto fb_mess = fbb.CreateString(mess);
    Message_S2CBuilder packet(fbb);
    packet.add_msg(fb_mess);
    auto message = packet.Finish();
    fbb.Finish(message);
    asio::write(s, asio::buffer(fbb.GetBufferPointer(), fbb.GetSize()));
}

int main(int argc, char* argv[])
{
    try {
        asio::io_context io_context;
        asio::ip::tcp::acceptor my_acceptor{ io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), PORT) };
        std::cout << "FlatBuffers echo server listening on port " << PORT << "\n";
        for (;;) {
            asio::ip::tcp::socket c_socket = my_acceptor.accept();
            std::cout << "Client connected.\n";
            try {
                for (;;) {
                    std::string mess = read_message(c_socket);
                    std::cout << mess.size() << " bytes received: " << mess << std::endl;
                    send_message(c_socket, mess);
                }
            }
            catch (std::exception& e) {
                // 클라이언트 종료(EOF) 또는 잘못된 데이터 -> 다음 접속 대기
                std::cout << "Client disconnected: " << e.what() << "\n";
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

