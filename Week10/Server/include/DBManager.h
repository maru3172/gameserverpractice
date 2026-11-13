#pragma once

#include "Common.h"
#include <atomic>
#include <concurrent_queue.h>
#include <sqlext.h>

enum DB_TYPE { DB_LOGIN, DB_SAVE_POSITION };

struct DBEvent {
	DB_TYPE type;
	int session_id;
	int user_id;
	short x, y;
};

// ODBC(DSN: 2026_Game_Test)로 로그인/위치 저장을 처리하는 DB 스레드.
// DB 연결에 실패하면 오프라인 모드로 동작해서 로그인은 항상 성공시키고 위치 저장은 생략한다.
class DBManager {
public:
	~DBManager();

	void request_login(int session_id, int user_id);
	void request_save_position(int user_id, short x, short y);

	void run(HANDLE iocp);    // DB 스레드 본체
	void stop() { m_running = false; }

private:
	bool connect();
	bool login_player(int user_id, DB_RESULT& result);
	void save_player_position(int user_id, short x, short y);
	static bool sql_success(SQLRETURN retcode);
	static void print_error(SQLSMALLINT handle_type, SQLHANDLE handle);

	concurrency::concurrent_queue<DBEvent> m_queue;
	SQLHENV m_henv = SQL_NULL_HENV;
	SQLHDBC m_hdbc = SQL_NULL_HDBC;
	std::atomic<bool> m_connected{ false };
	std::atomic<bool> m_running{ true };
};
