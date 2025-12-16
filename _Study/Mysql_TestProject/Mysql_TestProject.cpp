#define WIN32_LEAN_AND_MEAN
#include <errmsg.h>
#include <iostream>
#include <mysql.h>

#include <strsafe.h>
#include <thread>
#include <timeapi.h>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "winmm.lib")

#pragma comment(lib, "Ws2_32.lib")

#define TABLE_TYPE 4
#pragma comment(lib, "libmysql.dll")

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/lib/CLockFreeMemoryPool/CLockFreeMemoryPool.h"
#include "Profiler_MultiThread.h"

// PATH=C:\Program Files\MySQL\MySQL Server 8.0\bin;C:\Program Files\MySQL\MySQL Server 8.0\lib;%PATH%
void DB_SaveThread(void *arg);
void LogicThread(void *arg);
void MonitorThread(void *arg);
void InitTables(MYSQL *connection);
inline std::string ToUtf8(const std::wstring &w);

HANDLE hDeleteEvent;
int DBThreadCnt;
LONG64 iMsgCount;
std::vector<HANDLE> hIocp_vec;
std::vector<std::thread> hDBThread_vec;
std::vector<ull> tpsvec;
LONG64 tpsIdx = -1;

LONG64 g_InsertCnt = 0;
LONG64 g_SearchCnt = 0;


std::string utf8_DBIPAddress;
std::string utf8_DBId;
std::string utf8_DBPassword;
std::string utf8_DBName;
USHORT DBPort;
int bAutoCommit = false;

struct stRAIIBegin
{
    stRAIIBegin(MYSQL* connection)
        : _connection(connection)
    {
        if (bTransaction == false)
            return;
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
        if (bTransaction == false)
            return;

        if (mysql_query(_connection, "Commit") != 0)
        {
            printf("START TRANSACTION error: %s\n", mysql_error(_connection));
            __debugbreak();
        }
    }
    inline static int bTransaction = false;
    MYSQL *_connection = nullptr;
};

struct stMyOverlapped : public OVERLAPPED
{
    enum en_MsgType
    {
        CDB_CreateAccount = 0,
        CDB_BroadInsert,
        CDB_SearchAccount,
        MAX,
    };
    en_MsgType m_Type;
};
CObjectPool<stMyOverlapped> OverlappedPool;

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
            AccountNo, 0, rand()%100);

        stRAIIBegin transaction(connection);
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

};
struct CDB_BroadInsert : public IJob
{

    void exe(MYSQL *connection)
    {

        const char *Insertformat[TABLE_TYPE] = {
            "INSERT INTO `test`.`player` (`AccountNo`) values(%d)",
            "insert into `test`.`log`  (`AccountNo`) values(%d)",
            "insert into `test`.`quest_complete`  (`AccountNo`) values(%d)",
            "insert into `test`.`quest_progress`  (`AccountNo`) values(%d)"
        };
        MYSQL_RES *sql_result;
        MYSQL_ROW sql_row;
        int query_stat;
        char query[1000];

        stRAIIBegin transaction(connection);
        for (int i = 0; i < TABLE_TYPE; i++)
        {
            StringCchPrintfA(query, sizeof(query), Insertformat[i], AccountNo);
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

         while ((sql_row = mysql_fetch_row(sql_result)) != NULL)
         {
             // 실제 컬럼이 int형 타입이었다. 우리가 이거를 문자열에서 다시 숫자로 변환시켜야 돼요
             //printf("%2d %2d %d\n", atoi(sql_row[0]), atoi(sql_row[1]), atoi(sql_row[2]));
         }
         mysql_free_result(sql_result);
        
    }
};

#include <conio.h>
#include <string>

int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        __debugbreak();
    HANDLE hIocp = nullptr;
    hDeleteEvent = CreateEvent(nullptr, false, false, nullptr);

    if (mysql_library_init(0, nullptr, nullptr))
    {
        __debugbreak(); // MySQL 라이브러리 초기화 실패
    }

    
    WCHAR DBIPAddress[16];
    WCHAR DBId[100];
    WCHAR DBPassword[100];
    WCHAR DBName[100];

    wchar_t ProfilerName[100];
   
    {
        Parser parser;

        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"DBThreadCnt", DBThreadCnt);
        parser.GetValue(L"DBIPAddress", DBIPAddress, 16);
        parser.GetValue(L"DBPort", DBPort);
        parser.GetValue(L"DBId", DBId, 100);
        parser.GetValue(L"DBPassword", DBPassword, 100);
        parser.GetValue(L"DBName", DBName, 100);
        parser.GetValue(L"bAutoCommit", bAutoCommit);
        parser.GetValue(L"ProfilerName", ProfilerName,100);
        parser.GetValue(L"bTransaction", stRAIIBegin::bTransaction);


        utf8_DBIPAddress = ToUtf8(DBIPAddress);
        utf8_DBId = ToUtf8(DBId);
        utf8_DBPassword = ToUtf8(DBPassword);
        utf8_DBName = ToUtf8(DBName);

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

    std::thread hEventThread(LogicThread, nullptr);
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
            if (ch == 'p' || ch == 'P')
            {
                Profiler::bOn = true;
            }
            else if (ch == 'a' || ch == 'A')
            {
                Profiler::SaveAsLog(ProfilerName);
            }
            else if (ch == 'd' || ch == 'D')
            {
                Profiler::Reset();
            }
        }
    }
    WSACleanup();
}
void InitTables(MYSQL *connection);

#define ORIGINAL_FRAME 20
DWORD frameTime = ORIGINAL_FRAME;
bool bOn = true;

inline std::string ToUtf8(const std::wstring &w)
{
    if (w.empty())
        return {};

    int len = WideCharToMultiByte(
        CP_UTF8, 0, w.data(), (int)w.size(),
        nullptr, 0, nullptr, nullptr);

    std::string s(len, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, w.data(), (int)w.size(),
        s.data(), len, nullptr, nullptr);
    return s;
}

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
    connection = mysql_real_connect(&conn, utf8_DBIPAddress.c_str(), utf8_DBId.c_str(), utf8_DBPassword.c_str(), utf8_DBName.c_str(), DBPort, (char *)NULL, 0);
    if (connection == NULL)
    {

        fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
        mysql_errno(&conn);
        __debugbreak();
        return;
    }
    
    if (!bAutoCommit)
    {
        if (mysql_autocommit(connection, 0) != 0)
            __debugbreak();
    }

    InitTables(connection);

    DWORD transfferd;
    ULONG64 key;
    OVERLAPPED *overalpped;

    LONG64 localtpsIdx = _interlockedincrement64(&tpsIdx);
    {
        LARGE_INTEGER freq{}, last{};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);

        uint64_t tickCount = 0; // 1초 구간 처리량

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
            stMyOverlapped *msgType = reinterpret_cast<stMyOverlapped*>(overalpped);
            switch (msgType->m_Type)
            {
            case stMyOverlapped::en_MsgType::CDB_CreateAccount:
            {
                Profiler profile(L"CreateAccount_exe");
                job->exe(connection);
                _InterlockedDecrement64(&iMsgCount);
            }
            break;
            case stMyOverlapped::en_MsgType::CDB_BroadInsert:
            {
                Profiler profile(L"BroadInsert_exe");
                job->exe(connection);
                _InterlockedDecrement64(&iMsgCount);
            }
            break;
            case stMyOverlapped::en_MsgType::CDB_SearchAccount:
            {
                Profiler profile(L"SearchAccount_exe");
                job->exe(connection);
                _InterlockedDecrement64(&iMsgCount);
            }
            break;
            default:
                __debugbreak();
            }

            delete job;
            OverlappedPool.Release(msgType);

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
        printf("============================================================\n");
        printf("[bAutoCommit] : %5d \t[bTransaction] : %5d \n", bAutoCommit, stRAIIBegin::bTransaction);
        printf("[DB] TPS: %5lld ( frameTime = %05d msgCnt = %05lld)\n",
               total, frameTime, iMsgCount);
        printf("============================================================\n");
        currentTime = timeGetTime();

        if (nextTime >= currentTime)
        {

            Sleep(nextTime - currentTime);
        }
    }
}
int accountNo = 0;
void Logic()
{
    
    int loopCnt = rand() % 500;
    LONG64 currentMsgCnt = 0;
    static bool bInsert = true;
    for (int i = 0; i < loopCnt; i++)
    {
        IJob *msg = nullptr;
        int idx = accountNo % DBThreadCnt;
        stMyOverlapped *overlapped = nullptr;

        if (bOn)
        {
            int msgType = rand() % stMyOverlapped::en_MsgType::MAX;
            switch (msgType)
            {
                case 0:
                {
                    msg = new CDB_CreateAccount();
                    msg->AccountNo = accountNo++;
                    overlapped = reinterpret_cast<stMyOverlapped *>(OverlappedPool.Alloc());
                    overlapped->m_Type = stMyOverlapped::en_MsgType::CDB_CreateAccount;
                }
                break;
            case 1:
                {
                    msg = new CDB_BroadInsert();
                    msg->AccountNo = accountNo++;
                    overlapped = reinterpret_cast<stMyOverlapped *>(OverlappedPool.Alloc());
                    overlapped->m_Type = stMyOverlapped::en_MsgType::CDB_BroadInsert;
                }
                break;
            case 2:
                {
                    msg = new CDB_SearchAccount();
                    msg->AccountNo = accountNo++;
                    overlapped = reinterpret_cast<stMyOverlapped *>(OverlappedPool.Alloc());
                    overlapped->m_Type = stMyOverlapped::en_MsgType::CDB_SearchAccount;
                }
                break;
            default:
                __debugbreak();
            }
            if (msg == nullptr || overlapped ==nullptr) 
                __debugbreak();
            PostQueuedCompletionStatus(hIocp_vec[idx], 0, (ULONG_PTR)msg, overlapped);
            currentMsgCnt = _InterlockedIncrement64(&iMsgCount);
        }

    }
    if (currentMsgCnt > 100000)
    {
        frameTime += 1;
    }
    else if (frameTime > ORIGINAL_FRAME )
    {
        frameTime -= 1;
    }
}

void LogicThread(void *arg)
{

    DWORD currentTime = timeGetTime();
    DWORD nextTime = currentTime;

    while (1)
    {
        nextTime += frameTime;
        Logic();
        currentTime = timeGetTime();
        if (nextTime > currentTime)
        {
            Sleep(nextTime - currentTime);
        }
    }
}

void InitTables(MYSQL *connection)
{
    int query_stat;
    char query[1000];
    const char *DeleteFormat[TABLE_TYPE] =
    {
        "DELETE FROM test.player",
        "DELETE FROM test.log",
        "DELETE FROM test.quest_complete",
        "DELETE FROM test.quest_progress",
    };
    for (int i = 0; i < TABLE_TYPE; i++)
    {
        query_stat = mysql_query(connection, DeleteFormat[i]);
        if (query_stat != 0)
        {
            printf("Mysql query error : %s", mysql_error(connection));
            __debugbreak();
        }
        my_ulonglong affected = mysql_affected_rows(connection);
        printf("Deleted rows: %llu\n", affected);
    }

}


