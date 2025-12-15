#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <mysql.h>
#include <errmsg.h>

#include <thread>
#include <strsafe.h>
#include <timeapi.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "winmm.lib")

#pragma comment(lib, "Ws2_32.lib")

#include "../../_3Course/lib/CLockFreeMemoryPool/CLockFreeMemoryPool.h"
#include "Profiler_MultiThread.h"
#include "../../_1Course/lib/Parser_lib/Parser_lib.h"

//PATH=C:\Program Files\MySQL\MySQL Server 8.0\bin;C:\Program Files\MySQL\MySQL Server 8.0\lib;%PATH%
void DB_SaveThread(void *arg);
void EventThread(void *arg);
void MonitorThread(void *arg);

int DBThreadCnt;
LONG64 iMsgCount;
std::vector<HANDLE> hIocp_vec;
std::vector<std::thread> hDBThread_vec;
std::vector<ull> tpsvec;
LONG64 tpsIdx = -1;

struct IJob
{
    virtual void exe(MYSQL *connection) {};
    int AccountNo;
};
struct CDB_CreateAccount :public IJob
{

    void exe(MYSQL* connection) 
    {
        int query_stat;
        char query[1000];
        StringCchPrintfA(query, sizeof(query), "INSERT INTO `test`.`player` (`AccountNo`, `Level`, `Money`) VALUES ('%d', '%d', '%d')", AccountNo,0,0);
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
    int AccountNo;
 
};

#include <conio.h>
#include <string>

int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        __debugbreak();
    HANDLE hIocp = nullptr;
    
      if (mysql_library_init(0, nullptr, nullptr))
    {
        __debugbreak(); // MySQL 라이브러리 초기화 실패
    }

    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"DBThreadCnt", DBThreadCnt);

        
    }

    hIocp_vec.reserve(DBThreadCnt);
    hDBThread_vec.reserve(DBThreadCnt);
    tpsvec.resize(DBThreadCnt);

    for (int i = 0; i < DBThreadCnt; i++)
    {
        hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (hIocp == NULL)
            __debugbreak();
        hIocp_vec.push_back(hIocp);
        hDBThread_vec.emplace_back(DB_SaveThread, hIocp);
        SetThreadDescription(hDBThread_vec.back().native_handle(), L"DBThread");
    }

    std::thread hEventThread(EventThread, &hIocp_vec);
    SetThreadDescription(hEventThread.native_handle(), L"EventThread");

    std::thread hMonitorThread(MonitorThread, nullptr);
    SetThreadDescription(hMonitorThread.native_handle(), L"MonitorThread");

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == VK_ESCAPE)
            {
                CloseHandle(hIocp);
            }

        }
    }
    WSACleanup();
}
void InitPlayerTable(MYSQL* connection);

#define ORIGINAL_FRAME 20
DWORD frameTime = ORIGINAL_FRAME;
bool bOn = true;

void DB_SaveThread(void *arg)
{
    MYSQL conn;
    MYSQL *connection = NULL;
    MYSQL_RES *sql_result;
    MYSQL_ROW sql_row;
    int query_stat;

    // 초기화
    mysql_init(&conn);

    // DB 연결
    connection = mysql_real_connect(&conn, "127.0.0.1", "root", "123123", "Test", 3306, (char *)NULL, 0);
    if (connection == NULL)
    {

        fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
        mysql_errno(&conn);
        __debugbreak();
        return;
    }

    InitPlayerTable(connection);

    // GQCS 로 할까?  결국 PQCS 로하면 똑같은거고,,,

    DWORD transfferd;
    ULONG64 key;
    OVERLAPPED *overalpped;

    LONG64 localtpsIdx = _interlockedincrement64(&tpsIdx);

    {
        //_Out_ LPDWORD lpNumberOfBytesTransferred,
        //    _Out_ PULONG_PTR lpCompletionKey,
        //    _Out_ LPOVERLAPPED *lpOverlapped,
    }
    {
        LARGE_INTEGER freq{}, last{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);

        uint64_t tickCount = 0;  // 1초 구간 처리량

        while (1)
        {
            {
                transfferd = 0;
                key = 0;
                overalpped = nullptr;
            }
            GetQueuedCompletionStatus(arg, &transfferd, &key, &overalpped, INFINITE);
            if (transfferd == 0 && key == 0 && overalpped == nullptr)
                break;
            IJob *job = reinterpret_cast<IJob *>(key);
            {
                Profiler profile(L"Job_exe");
                job->exe(connection);
                _InterlockedDecrement64(&iMsgCount);
            }

            delete job;

            {
                // TPS 카운트
                ++tickCount;

                // 1초 경과 시 TPS 계산
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);

                double elapsedSec = double(now.QuadPart - last.QuadPart) / double(freq.QuadPart);
                if (elapsedSec >= 1.0)
                {
                    double tps = double(tickCount) / elapsedSec;

                    tpsvec[localtpsIdx] = tickCount;
                    // 다음 구간 리셋
                    tickCount = 0;
                    last = now;
                }
            }
        }
    }
 
}
void MonitorThread(void *arg) 
{
    DWORD currentTime = timeGetTime();
    DWORD nextTime = currentTime;

    while (1)
    {
        nextTime += 1000;
        ull total = 0;

        for (int i = 0; i < DBThreadCnt; i++)
        {
            total += tpsvec[i];
        }
        printf("[DB] TPS: %5lld ( frameTime = %05d msgCnt = %05lld)\n",
               total, frameTime, iMsgCount);
        currentTime = timeGetTime();

        if (nextTime >= currentTime)
        {

            Sleep(nextTime - currentTime);
        }

    }
}
void Logic(void *arg)
{
    static int accountNo = 0;
    int loopCnt = rand() % 500;
    LONG64 currentMsgCnt = 0;

    for (int i = 0; i < loopCnt; i++)
    {
        CDB_CreateAccount *msg = new CDB_CreateAccount();
        msg->AccountNo = accountNo++;
        if (bOn)
        {
            int idx = accountNo % DBThreadCnt;

            PostQueuedCompletionStatus(hIocp_vec[idx], 0, (ULONG_PTR)msg, nullptr);
            currentMsgCnt = _InterlockedIncrement64(&iMsgCount);
        }

    }
    if (currentMsgCnt > 100000)
    {
        frameTime += 1;
    }
    else if (frameTime > ORIGINAL_FRAME)
    {
        frameTime = ORIGINAL_FRAME;
    }
}

void EventThread(void *arg)
{
    
    DWORD currentTime = timeGetTime();
    DWORD nextTime = currentTime;

    while (1)
    {
        nextTime += frameTime;
        Logic(arg);
        currentTime = timeGetTime();
        if (nextTime > currentTime)
        {
            Sleep(nextTime - currentTime);
        }
    }
}

void InitPlayerTable(MYSQL* connection)
{
    int query_stat;
    char query[1000];
    query_stat = mysql_query(connection, "DELETE FROM test.player");
    if (query_stat != 0)
    {
        printf("Mysql query error : %s", mysql_error(connection));
        __debugbreak();
    }
    my_ulonglong affected = mysql_affected_rows(connection);
    printf("Deleted rows: %llu\n", affected);

}
