#pragma once
#include <iostream>

#include "C:\Program Files\MySQL\MySQL Server 8.0\include\mysql.h"

#include "DB.h"
#include <strsafe.h>

#define TABLE_TYPE 4
extern int ASyncMode;


struct IJob
{
    virtual void exe(CDB *db) {};
};
struct CDB_CreateAccount : public IJob
{

    void exe(CDB *db)
    {
        stRAIIBegin transaction(db);
       CDB::ResultSet rs = db->Query("INSERT INTO `sys`.`player` (`AccountNo`, `Level`, `Money`) VALUES ('%d', '%d', '%d')", AccountNo, 0, rand() % 100);
        if (!rs.Sucess())
        {
            printf("Query Error %s \n", rs.Error().c_str());
            __debugbreak();
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

    void exe(CDB *db)
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
           CDB::ResultSet rs = db->Query(Insertformat[i], AccountNo);
            if (!rs.Sucess())
            {
                printf("Query Error %s \n", rs.Error().c_str());
                __debugbreak();
            }
        }
    }
    int AccountNo;
};
struct CDB_AllDelete : public IJob
{

    void exe(CDB *db)
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
            CDB::ResultSet rs = db->Query(DeleteFormat[i]);
            if (!rs.Sucess())
            {
                printf("Query Error %s \n", rs.Error().c_str());
                __debugbreak();
            }

        }
    }
    int AccountNo;
};
struct CDB_SearchAccount : public IJob
{

    void exe(CDB *db)
    {
        // stRAIIBegin transaction(db);
       CDB::ResultSet rs = db->Query("SELECT * FROM player WHERE AccountNo = %d", AccountNo);
        if (!rs.Sucess())
        {
            printf("Query Error %s \n", rs.Error().c_str());
            __debugbreak();
        }

        for (const auto &row : rs)
        {
            int acc = row["AccountNo"].AsInt();

        }
    }
    int AccountNo;
};
