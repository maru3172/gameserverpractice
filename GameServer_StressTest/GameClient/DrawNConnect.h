#pragma once
#include "OBJECT.h"

extern OBJECT avatar;
extern OBJECT white_tile, black_tile;

void client_initialize();

void client_finish();

void send_packet(void* packet);

void ProcessPacket(char* ptr);

void process_data(char* net_buf, size_t io_byte);

void client_main(); 
