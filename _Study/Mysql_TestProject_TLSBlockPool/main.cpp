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
#include "IJob.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "libmysql.dll")

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_4Course/_lib/CTlsBlockPool/CTlsBlockPool.h"
#include "../../_3Course/lib/CSystemLog_lib/CSystemLog_lib.h"

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
int ASyncMode = 0;

thread_local CTlsBlockPool pool;
CTlsBlockPoolManager *IJobManagerPool;


struct stMyOverlapped : public OVERLAPPED
{
    enum en_MsgType
    {
        CDB_CreateAccount = 0,
        CDB_BroadInsert,
        CDB_SearchAccount,
        MAX,
        CDB_AllDelete,
    };
    en_MsgType m_Type;
};

CObjectPool<stMyOverlapped> OverlappedPool;


#include <conio.h>
#include <string>

int main()
{
    // ManagerPool 초기화
    {
        int capacity = 300;
        int blockSize = max(max(sizeof(CDB_CreateAccount), sizeof(CDB_BroadInsert)), sizeof(CDB_SearchAccount));

        IJobManagerPool = new CTlsBlockPoolManager(capacity, blockSize);
    }
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
        parser.GetValue(L"ASyncMode", ASyncMode);


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
                //CloseHandle(hIocp);
                break;
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

thread_local DB db;
void DB_SaveThread(void *arg)
{
    db.Connect(utf8_DBIPAddress.c_str(), utf8_DBId.c_str(), utf8_DBPassword.c_str(), utf8_DBName.c_str(), DBPort);
    pool.InitTlsPool(IJobManagerPool);
     
    {
        CDB_AllDelete *msg = pool.Alloc<CDB_AllDelete>();
        msg->exe(&db);
        pool.Release<CDB_CreateAccount>(msg);
    }

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

                job->exe(&db);
                _InterlockedDecrement64(&iMsgCount);

                pool.Release<CDB_CreateAccount>(job);
            }
            break;
            case stMyOverlapped::en_MsgType::CDB_BroadInsert:
            {
       
                job->exe(&db);
                _InterlockedDecrement64(&iMsgCount);
                pool.Release<CDB_BroadInsert>(job);
            }
            break;
            case stMyOverlapped::en_MsgType::CDB_SearchAccount:
            {
                job->exe(&db);
                _InterlockedDecrement64(&iMsgCount);
                pool.Release<CDB_SearchAccount>(job);
            }
            break;
            case stMyOverlapped::en_MsgType::CDB_AllDelete:
            {
                job->exe(&db);
                _InterlockedDecrement64(&iMsgCount);
                pool.Release<CDB_SearchAccount>(job);
            }
            break;
            default:
                __debugbreak();
            }

            //delete job;
            OverlappedPool.Release(msgType);

            {
                // TPS 카운트
                tpsvec[localtpsIdx]++;
            }
        }
    }
    __debugbreak();
}
void MonitorThread(void *arg)
{
    DWORD currentTime = timeGetTime();
    DWORD nextTime = currentTime;
    std::vector<ull> beforeTps;
    beforeTps.resize(DBThreadCnt);
    while (1)
    {
        nextTime += 1000;
        ull total = 0;

        for (int i = 0; i < DBThreadCnt; i++)
        {
            ull cur = tpsvec[i];
            ull delta = cur - beforeTps[i];
            beforeTps[i] = cur;

            total += delta;
        }
        printf("============================================================\n");
        printf("[bAutoCommit] : %5d \t \n", bAutoCommit);
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
        
        int idx = accountNo % DBThreadCnt;
        stMyOverlapped *overlapped = nullptr;

        if (bOn)
        {
            int msgType = rand() % stMyOverlapped::en_MsgType::MAX;
            switch (msgType)
            {
                case 0:
                {
                    Profiler profile(L"CDB_CreateAccount");
                    CDB_CreateAccount* msg = pool.Alloc<CDB_CreateAccount>();
                    //msg = new CDB_CreateAccount();
                    
                    msg->AccountNo = accountNo++;
                    overlapped = reinterpret_cast<stMyOverlapped *>(OverlappedPool.Alloc());
                    overlapped->m_Type = stMyOverlapped::en_MsgType::CDB_CreateAccount;
                    PostQueuedCompletionStatus(hIocp_vec[idx], 0, (ULONG_PTR)msg, overlapped);
                }
                break;
            case 1:
                {
                    Profiler profile(L"CDB_BroadInsert");
                    CDB_BroadInsert* msg = pool.Alloc<CDB_BroadInsert>();

                    //msg = new CDB_BroadInsert();
                    msg->AccountNo = accountNo++;
                    overlapped = reinterpret_cast<stMyOverlapped *>(OverlappedPool.Alloc());
                    overlapped->m_Type = stMyOverlapped::en_MsgType::CDB_BroadInsert;
                    PostQueuedCompletionStatus(hIocp_vec[idx], 0, (ULONG_PTR)msg, overlapped);
                }
                break;
            case 2:
                {
                    Profiler profile(L"CDB_SearchAccount");
                    CDB_SearchAccount* msg = pool.Alloc<CDB_SearchAccount>();

                    //msg = new CDB_SearchAccount();
                    msg->AccountNo = accountNo++;
                    overlapped = reinterpret_cast<stMyOverlapped *>(OverlappedPool.Alloc());
                    overlapped->m_Type = stMyOverlapped::en_MsgType::CDB_SearchAccount;
                    PostQueuedCompletionStatus(hIocp_vec[idx], 0, (ULONG_PTR)msg, overlapped);
                }
                break;
            default:
                __debugbreak();
            }

            currentMsgCnt = _InterlockedIncrement64(&iMsgCount);
        }

    }
    if (currentMsgCnt > 100000)
    {
        frameTime += 10;
    }
    else if (frameTime > ORIGINAL_FRAME )
    {
        frameTime -= 10;
        if (frameTime < ORIGINAL_FRAME)
            frameTime = ORIGINAL_FRAME;
    }
}

void LogicThread(void *arg)
{

    DWORD currentTime = timeGetTime();
    DWORD nextTime = currentTime;
    pool.InitTlsPool(IJobManagerPool);
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



