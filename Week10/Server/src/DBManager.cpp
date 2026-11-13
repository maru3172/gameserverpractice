#include "DBManager.h"
#include <iostream>
#include <thread>

#pragma comment(lib, "odbc32.lib")

using namespace std::chrono;

DBManager::~DBManager()
{
	if (m_hdbc != SQL_NULL_HDBC) {
		SQLDisconnect(m_hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
	}
	if (m_henv != SQL_NULL_HENV)
		SQLFreeHandle(SQL_HANDLE_ENV, m_henv);
}

void DBManager::request_login(int session_id, int user_id)
{
	m_queue.push(DBEvent{ DB_LOGIN, session_id, user_id, 0, 0 });
}

void DBManager::request_save_position(int user_id, short x, short y)
{
	if (!m_connected) return;
	m_queue.push(DBEvent{ DB_SAVE_POSITION, -1, user_id, x, y });
}

bool DBManager::sql_success(SQLRETURN retcode)
{
	return retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO;
}

void DBManager::print_error(SQLSMALLINT handle_type, SQLHANDLE handle)
{
	SQLSMALLINT rec = 0;
	SQLINTEGER native_error;
	SQLWCHAR state[SQL_SQLSTATE_SIZE + 1];
	SQLWCHAR message[1000];

	while (SQLGetDiagRecW(handle_type, handle, ++rec, state, &native_error,
		message, 1000, NULL) == SQL_SUCCESS)
		std::wcout << L"[" << state << L"] " << message << L" (" << native_error << L")\n";
}

bool DBManager::connect()
{
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);
	if (!sql_success(retcode)) return false;

	retcode = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (!sql_success(retcode)) {
		print_error(SQL_HANDLE_ENV, m_henv);
		return false;
	}

	retcode = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);
	if (!sql_success(retcode)) return false;

	SQLSetConnectAttr(m_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
	retcode = SQLConnectW(m_hdbc, (SQLWCHAR*)L"2026_Game_Test", SQL_NTS, NULL, 0, NULL, 0);
	if (!sql_success(retcode)) {
		print_error(SQL_HANDLE_DBC, m_hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
		m_hdbc = SQL_NULL_HDBC;
		return false;
	}
	return true;
}

bool DBManager::login_player(int user_id, DB_RESULT& result)
{
	SQLHSTMT hstmt;
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);
	if (!sql_success(retcode)) return false;

	SQLWCHAR query[100];
	swprintf_s(query, L"EXEC dbo.login_player @UserId = %d", user_id);
	retcode = SQLExecDirectW(hstmt, query, SQL_NTS);
	if (!sql_success(retcode)) {
		print_error(SQL_HANDLE_STMT, hstmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
		return false;
	}

	SQLINTEGER db_user_id = -1;
	SQLSMALLINT user_x = 0;
	SQLSMALLINT user_y = 0;
	SQLLEN cb_user_id = 0, cb_x = 0, cb_y = 0;

	SQLBindCol(hstmt, 1, SQL_C_LONG, &db_user_id, sizeof(db_user_id), &cb_user_id);
	SQLBindCol(hstmt, 3, SQL_C_SSHORT, &user_x, sizeof(user_x), &cb_x);
	SQLBindCol(hstmt, 4, SQL_C_SSHORT, &user_y, sizeof(user_y), &cb_y);

	retcode = SQLFetch(hstmt);
	if (sql_success(retcode)) {
		result.success = true;
		result.user_id = db_user_id;
		result.has_position = cb_x != SQL_NULL_DATA && cb_y != SQL_NULL_DATA;
		result.x = user_x;
		result.y = user_y;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	return result.success;
}

void DBManager::save_player_position(int user_id, short x, short y)
{
	SQLHSTMT hstmt;
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hstmt);
	if (!sql_success(retcode)) return;

	SQLWCHAR query[120];
	swprintf_s(query, L"EXEC dbo.set_player_position @UserId = %d, @X = %d, @Y = %d", user_id, x, y);
	retcode = SQLExecDirectW(hstmt, query, SQL_NTS);
	if (!sql_success(retcode))
		print_error(SQL_HANDLE_STMT, hstmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

void DBManager::run(HANDLE iocp)
{
	m_connected = connect();
	if (m_connected)
		std::cout << "[DB] Connected to DSN 2026_Game_Test.\n";
	else
		std::cout << "[DB] Connection failed. Running in OFFLINE mode (login always succeeds, positions are not saved).\n";

	while (m_running) {
		DBEvent ev;
		if (!m_queue.try_pop(ev)) {
			std::this_thread::sleep_for(1ms);
			continue;
		}

		if (ev.type == DB_LOGIN) {
			EXP_OVER* db_over = new EXP_OVER(IO_DB_LOGIN);
			if (m_connected) {
				login_player(ev.user_id, db_over->m_db_result);
			}
			else {
				db_over->m_db_result.success = true;
				db_over->m_db_result.user_id = ev.user_id;
				db_over->m_db_result.has_position = false;
			}
			PostQueuedCompletionStatus(iocp, 1, ev.session_id, &db_over->m_over);
		}
		else if (m_connected) {
			save_player_position(ev.user_id, ev.x, ev.y);
		}
	}
}
