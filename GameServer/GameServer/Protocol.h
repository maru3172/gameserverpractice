#pragma once

constexpr short PORT = 3500;
constexpr int WORLD_WIDTH = 8;
constexpr int WORLD_HEIGHT = 8;
constexpr int MAX_PLAYERS = 10;
constexpr int MAX_NAME_LEN = 20;

enum PACKET_TYPE { C2S_LOGIN, C2S_MOVE, S2C_LOGIN_RESULT, S2C_AVATAR_INFO, S2C_ADD_PLAYER, S2C_REMOVE_PLAYER, S2C_MOVE_PLAYER };
enum DIRECTION { UP, DOWN, LEFT, RIGHT };

#pragma pack(push, 1)
// 접속 요청, 클라이언트 -> 서버
struct C2S_Login
{
	unsigned char	size;
	PACKET_TYPE		type;
	char			username[MAX_NAME_LEN]; // 누가 접속했는지
};

// 이동 전송, 클라이언트 -> 서버
struct C2S_Move
{
	unsigned char	size;
	PACKET_TYPE		type;
	DIRECTION		dir; // 이동 방향
};

// 로그인 성공 여부, 서버 -> 클라이언트
struct S2C_LoginResult
{
	unsigned char	size;
	PACKET_TYPE		type;
	bool			success;
	char			message[50];
};

// 내가 접속 시작한 정보, 서버 -> 클라이언트
struct S2C_AvatarInfo
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
	short			x;
	short			y;
};

// 다른 플레이어 접속 정보, 서버-> 클라이언트
struct S2C_AddPlayer
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
	char			username[MAX_NAME_LEN];
	short			x;
	short			y;
};

// 플레이어 나감, 서버 -> 클라이언트
struct S2C_RemovePlayer
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
};

// 플레이어 이동 동기화, 서버 -> 클라이언트
struct S2C_MovePlayer
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
	short			x;
	short			y;
};

#pragma pop
