// CDB.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "CDB.h"
#include <iostream>

// TODO: 라이브러리 함수의 예제입니다.
thread_local CDB db;
void fnCDB()
{
    db.Connect("127.0.0.1", "root", "123123", "test"); //port

    stRAIIBegin transaction(&db);//트랜잭션을 희망할 경우.
    int AccountNo = 10;

    CDB::ResultSet rs = db.Query("INSERT INTO `sys`.`player` (`AccountNo`, `Level`, `Money`) VALUES ('%d', '%d', '%d')", AccountNo, 0, rand() % 100);
    if (!rs.Sucess())
    {
        printf("Query Error %s \n", rs.Error().c_str());
        __debugbreak();
    }

    //select의 경우
    for (const auto &row : rs)
    {
        int acc = row["AccountNo"].AsInt();
    }
}

