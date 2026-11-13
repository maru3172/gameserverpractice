-- Week10 서버용 DB 설정 스크립트 (SQL Server / MSSQL)
-- 실행: sqlcmd -S .\SQLEXPRESS -E -i setup.sql
-- 서버 코드(DBManager.cpp)가 기대하는 형식:
--   EXEC dbo.login_player @UserId = N   -> 1열 user_id, 3열 x, 4열 y (x,y 가 NULL 이면 위치 없음 = 랜덤 스폰)
--   EXEC dbo.set_player_position @UserId = N, @X = x, @Y = y

IF DB_ID(N'GameServerDB') IS NULL
    CREATE DATABASE GameServerDB;
GO

USE GameServerDB;
GO

IF OBJECT_ID(N'dbo.user_table', N'U') IS NULL
BEGIN
    CREATE TABLE dbo.user_table (
        user_id   INT          NOT NULL PRIMARY KEY,
        user_name NVARCHAR(20) NOT NULL,
        user_x    SMALLINT     NULL,
        user_y    SMALLINT     NULL
    );
END
GO

-- 로그인: 처음 보는 ID 는 계정을 자동 생성하고(위치 NULL), 계정 정보를 반환한다.
CREATE OR ALTER PROCEDURE dbo.login_player
    @UserId INT
AS
BEGIN
    SET NOCOUNT ON;   -- INSERT 의 "n개 행" 메시지가 결과 집합 앞에 끼지 않도록

    IF NOT EXISTS (SELECT 1 FROM dbo.user_table WHERE user_id = @UserId)
        INSERT INTO dbo.user_table (user_id, user_name, user_x, user_y)
        VALUES (@UserId, CAST(@UserId AS NVARCHAR(20)), NULL, NULL);

    SELECT user_id, user_name, user_x, user_y
    FROM dbo.user_table
    WHERE user_id = @UserId;
END
GO

-- 위치 저장
CREATE OR ALTER PROCEDURE dbo.set_player_position
    @UserId INT,
    @X SMALLINT,
    @Y SMALLINT
AS
BEGIN
    SET NOCOUNT ON;
    UPDATE dbo.user_table SET user_x = @X, user_y = @Y WHERE user_id = @UserId;
END
GO
