#include <iostream>
#include <mysql.h>
#include <errmsg.h>
#include <windows.h>
#include <thread>
#include <strsafe.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

#include "../../_3Course/lib/CLockFreeMemoryPool/CLockFreeMemoryPool.h"
#include "Profiler_MultiThread.h"

//PATH=C:\Program Files\MySQL\MySQL Server 8.0\bin;C:\Program Files\MySQL\MySQL Server 8.0\lib;%PATH%
void DB_SaveThread(void *arg);
void EventThread(void *arg);


LONG64 iMsgCount;

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

int main()
{
    HANDLE hIocp = nullptr;
    
    hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (hIocp == INVALID_HANDLE_VALUE)
        __debugbreak();

    std::thread hDBThread(DB_SaveThread,hIocp);
    SetThreadDescription(hDBThread.native_handle(), L"DBThread");

    std::thread hEventThread(EventThread, hIocp);
    SetThreadDescription(hEventThread.native_handle(), L"EventThread");

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
                    printf("[DB] TPS: %.1f ( frameTime = %05d msgCnt = %05lld)\n",
                           tps, frameTime , iMsgCount);

                    // 다음 구간 리셋
                    tickCount = 0;
                    last = now;
                }
            }
        }
    }
 
}

void Logic(void* arg) 
{
    static int accountNo = 0;
    int loopCnt = rand() % 500;
    LONG64 currentMsgCnt;
    for (int i = 0; i < loopCnt; i++)
    {
        CDB_CreateAccount *msg = new CDB_CreateAccount();
        msg->AccountNo = accountNo++;
        if (bOn)
        {
            PostQueuedCompletionStatus(arg, 0, (ULONG_PTR)msg, nullptr);
            currentMsgCnt = _InterlockedIncrement64(&iMsgCount);
            if (currentMsgCnt > 100000)
            {
                frameTime += 100;
            }
            else if (frameTime > ORIGINAL_FRAME)
            {
                frameTime = ORIGINAL_FRAME;
            }
        }
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
