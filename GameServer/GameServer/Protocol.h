#pragma once

constexpr short PORT = 3500;
constexpr int WORLD_WIDTH = 8;
constexpr int WORLD_HEIGHT = 8;
constexpr int MAX_PLAYERS = 10;
constexpr int MAX_NAME_LEN = 20;

enum PACKET_TYPE { C2S_LOGIN, C2S_MOVE, S2C_LOGIN_RESULT, S2C_AVATAR_INFO, S2C_ADD_PLAYER, S2C_REMOVE_PLAYER, S2C_MOVE_PLAYER };
enum DIRECTION { UP, DOWN, LEFT, RIGHT };

#pragma pack(push, 1)
// ���� ��û, Ŭ���̾�Ʈ -> ����
struct C2S_Login
{
	unsigned char	size;
	PACKET_TYPE		type;
	char			username[MAX_NAME_LEN]; // ���� �����ߴ���
};

// �̵� ����, Ŭ���̾�Ʈ -> ����
struct C2S_Move
{
	unsigned char	size;
	PACKET_TYPE		type;
	DIRECTION		dir; // �̵� ����
};

// �α��� ���� ����, ���� -> Ŭ���̾�Ʈ
struct S2C_LoginResult
{
	unsigned char	size;
	PACKET_TYPE		type;
	bool			success;
	char			message[50];
};

// ���� ���� ������ ����, ���� -> Ŭ���̾�Ʈ
struct S2C_AvatarInfo
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
	short			x;
	short			y;
};

// �ٸ� �÷��̾� ���� ����, ����-> Ŭ���̾�Ʈ
struct S2C_AddPlayer
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
	char			username[MAX_NAME_LEN];
	short			x;
	short			y;
};

// �÷��̾� ����, ���� -> Ŭ���̾�Ʈ
struct S2C_RemovePlayer
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
};

// �÷��̾� �̵� ����ȭ, ���� -> Ŭ���̾�Ʈ
struct S2C_MovePlayer
{
	unsigned char	size;
	PACKET_TYPE		type;
	int				playerId;
	short			x;
	short			y;
};

#pragma pack(pop)
