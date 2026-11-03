#pragma once
#include "OBJECT.h"

extern OBJECT avatar;
extern OBJECT white_tile, black_tile;
extern std::unordered_map <int, OBJECT> players;

// ù ���� �� �׷��ִ� �Լ�
void client_initialize();

// Ŭ���̾�Ʈ ���� ����
void client_finish();

// ��Ŷ �۽�
void send_packet(void* packet);

void ProcessPacket(char* ptr);

void process_data(unsigned char* net_buf, size_t io_byte);

void client_main(); 
