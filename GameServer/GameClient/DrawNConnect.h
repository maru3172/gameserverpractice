#pragma once
#include "OBJECT.h"

extern OBJECT avatar, players[MAX_USER];
extern OBJECT white_tile, black_tile;

// ù ���� �� �׷��ִ� �Լ�
void client_initialize();

// Ŭ���̾�Ʈ ���� ����
void client_finish();

// ��Ŷ �۽�
void send_packet(void* packet);

void ProcessPacket(char* ptr);

void process_data(char* net_buf, size_t io_byte);

void client_main(); 
