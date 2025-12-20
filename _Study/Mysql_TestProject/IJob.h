#pragma once
#include <mysql.h>
#include <iostream>
#include <strsafe.h>

#include "Profiler_MultiThread.h"

#define TABLE_TYPE 4

struct stRAIIBegin
{
    stRAIIBegin(MYSQL *connection)
        : _connection(connection)
    {

        if (_connection == nullptr)
        {
            __debugbreak();
            return;
        }
        if (mysql_query(_connection, "BEGIN") != 0)
        {
            printf("START TRANSACTION error: %s\n", mysql_error(_connection));
            __debugbreak();
        }
    }
    ~stRAIIBegin()
    {
        if (mysql_query(_connection, "Commit") != 0)
        {
            printf("START TRANSACTION error: %s\n", mysql_error(_connection));
            __debugbreak();
        }
    }

    MYSQL *_connection = nullptr;
};


struct IJob
{
    virtual void exe(MYSQL *connection) {};
    int AccountNo;
};
struct CDB_CreateAccount : public IJob
{

    void exe(MYSQL *connection)
    {
        int query_stat;
        char query[1000];

        StringCchPrintfA(query, sizeof(query), "INSERT INTO `test`.`player` (`AccountNo`, `Level`, `Money`) VALUES ('%d', '%d', '%d')",
                         AccountNo, 0, rand() % 100);

        stRAIIBegin transaction(connection);
        {
            Profiler profile(L"CreateAccount_exe");
            query_stat = mysql_query(connection, query);
            if (query_stat != 0)
            {
                printf("Mysql query error : %s", mysql_error(connection));
                __debugbreak();
            }
            my_ulonglong affected = mysql_affected_rows(connection);
            if (affected != 1)
            {
                printf("Unexpected insert count: %llu\n", affected);
            }
        }
    }
};
struct CDB_BroadInsert : public IJob
{

    void exe(MYSQL *connection)
    {

        const char *Insertformat[TABLE_TYPE] = {
            "INSERT INTO `test`.`player` (`AccountNo`) values(%d)",
            "insert into `test`.`log`  (`AccountNo`) values(%d)",
            "insert into `test`.`quest_complete`  (`AccountNo`) values(%d)",
            "insert into `test`.`quest_progress`  (`AccountNo`) values(%d)"};
        MYSQL_RES *sql_result;
        MYSQL_ROW sql_row;
        int query_stat;
        char query[1000];

        stRAIIBegin transaction(connection);
        for (int i = 0; i < TABLE_TYPE; i++)
        {
            StringCchPrintfA(query, sizeof(query), Insertformat[i], AccountNo);
            {
                Profiler profile(L"BroadInsert_exe");
                query_stat = mysql_query(connection, query);
                if (query_stat != 0)
                {
                    printf("Mysql query error : %s", mysql_error(connection));
                    __debugbreak();
                }
                my_ulonglong affected = mysql_affected_rows(connection);
                if (affected != 1)
                {
                    printf("Unexpected insert count: %llu\n", affected);
                }
            }
        }
    }
};
struct CDB_SearchAccount : public IJob
{

    void exe(MYSQL *connection)
    {
        MYSQL_RES *sql_result;
        MYSQL_ROW sql_row;
        int query_stat;
        char query[1000];
        stRAIIBegin transaction(connection);

        StringCchPrintfA(query, sizeof(query), "SELECT * FROM player WHERE AccountNo = %d", AccountNo);
        {
            Profiler profile(L"SearchAccount_exe");
            query_stat = mysql_query(connection, query);
            if (query_stat != 0)
            {
                printf("Mysql query error : %s", mysql_error(connection));
                __debugbreak();
            }

            // 결과출력
            sql_result = mysql_store_result(connection); // 결과 전체를 미리 가져옴
                                                         //	sql_result=mysql_use_result(connection);		// fetch_row 호출시 1개씩 가져옴
            sql_row = mysql_fetch_row(sql_result);
        }
        while ((sql_row = mysql_fetch_row(sql_result)) != NULL)
        {
            // 실제 컬럼이 int형 타입이었다. 우리가 이거를 문자열에서 다시 숫자로 변환시켜야 돼요
            // printf("%2d %2d %d\n", atoi(sql_row[0]), atoi(sql_row[1]), atoi(sql_row[2]));
        }
        mysql_free_result(sql_result);
    }
};
