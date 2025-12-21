#pragma once
#include <iostream>

#include "C:\Program Files\MySQL\MySQL Server 8.0\include\mysql.h"

#include "DB.h"
#include <strsafe.h>

#define TABLE_TYPE 4
extern int ASyncMode;

struct stRAIIBegin
{
    stRAIIBegin(DB *db)
        : _connection(db->_conn)
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
    virtual void exe(DB *db) {};
};
struct CDB_CreateAccount : public IJob
{

    void exe(DB *db)
    {
        stRAIIBegin transaction(db);
        DB::ResultSet rs = db->Query("INSERT INTO `sys`.`player` (`AccountNo`, `Level`, `Money`) VALUES ('%d', '%d', '%d')", AccountNo, 0, rand() % 100);
        if (!rs.Ok())
        {
            /* rs.Error() */
        }

        for (const auto &row : rs)
        {
            int acc = row["AccountNo"].AsInt();
        }
    }
    int AccountNo;
};
struct CDB_BroadInsert : public IJob
{

    void exe(DB *db)
    {

        const char *Insertformat[TABLE_TYPE] =
            {
                "INSERT INTO `sys`.`player` (`AccountNo`) values(%d)",
                "insert into `sys`.`log`  (`AccountNo`) values(%d)",
                "insert into `sys`.`quest_complete`  (`AccountNo`) values(%d)",
                "insert into `sys`.`quest_progress`  (`AccountNo`) values(%d)"};

        int query_stat;
        char query[1000];

        stRAIIBegin transaction(db);
        for (int i = 0; i < TABLE_TYPE; i++)
        {
            DB::ResultSet rs = db->Query(Insertformat[i], AccountNo);
            if (!rs.Ok())
            {
                /* rs.Error() */
            }

            for (const auto &row : rs)
            {
                int acc = row["AccountNo"].AsInt();
            }
        }
    }
    int AccountNo;
};
struct CDB_AllDelete : public IJob
{

    void exe(DB *db)
    {

        const char *DeleteFormat[TABLE_TYPE] =
            {
                "DELETE FROM sys.player",
                "DELETE FROM sys.log",
                "DELETE FROM sys.quest_complete",
                "DELETE FROM sys.quest_progress",
            };
        for (int i = 0; i < TABLE_TYPE; i++)
        {
            DB::ResultSet rs = db->Query(DeleteFormat[i]);
            if (!rs.Ok())
            {
                /* rs.Error() */
            }

            for (const auto &row : rs)
            {
                int acc = row["AccountNo"].AsInt();
            }
        }
    }
    int AccountNo;
};
struct CDB_SearchAccount : public IJob
{

    void exe(DB *db)
    {
        // stRAIIBegin transaction(db);
        DB::ResultSet rs = db->Query("SELECT * FROM player WHERE AccountNo = %d", AccountNo);
        if (!rs.Ok())
        {
            /* rs.Error() */
        }

        for (const auto &row : rs)
        {
            int acc = row["AccountNo"].AsInt();
        }
    }
    int AccountNo;
};
