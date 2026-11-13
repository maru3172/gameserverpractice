#pragma once
// 서버 / 클라이언트 / 스트레스 테스트가 공유하는 프로토콜 정의.
// 패킷 형식: [총 크기 1byte][패킷 ID 1byte][protobuf 직렬화 데이터]

constexpr int MAX_ID_LEN = 50;
constexpr int MAX_STR_LEN = 255;

constexpr int WORLD_WIDTH = 400;
constexpr int WORLD_HEIGHT = 400;

constexpr int NPC_ID_START = 20000;
constexpr unsigned short SERVER_PORT = 9000;

constexpr int PACKET_HEADER_SIZE = 2;
constexpr int MAX_PACKET_SIZE = 255;

constexpr unsigned char PKID_SC_LOGIN_OK = 1;
constexpr unsigned char PKID_SC_POS = 2;
constexpr unsigned char PKID_SC_PUT_OBJECT = 3;
constexpr unsigned char PKID_SC_REMOVE_OBJECT = 4;
constexpr unsigned char PKID_CS_LOGIN = 5;
constexpr unsigned char PKID_CS_MOVE = 6;

constexpr int DIR_TELEPORT = 99;    // CS_MOVE 의 dir 로 보내면 랜덤 위치로 순간이동
