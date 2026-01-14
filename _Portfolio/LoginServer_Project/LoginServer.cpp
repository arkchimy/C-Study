#include "LoginServer.h"
#include <Windows.h>
#include <timeapi.h>


#include "../../_4Course/_lib/CDB/CDB.h"

#include <cpp_redis/cpp_redis>
#include <iostream>
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "ws2_32.lib")

#include <Pdh.h>
#include <stdio.h>

#pragma comment(lib, "Pdh.lib")
#include "./CNetworkLib/utility/CCpuUsage/CCpuUsage.h"

#include "MonitorData.h"

// Chatting Server와의 연결
// Redis와의 연결
// Client와의 연결

// OnRecv를 통해 Player객체에 접근 Player에 Overapped 을 두고 
// 

extern thread_local stTlsLockInfo tls_LockInfo;
thread_local CDB db;
thread_local cpp_redis::client* client;
static ull cnt = 0;

enum en_PACKET_CS_LOGIN_RES_LOGIN : BYTE
{
    dfLOGIN_STATUS_NONE = -1,        // 미인증상태
    dfLOGIN_STATUS_FAIL = 0,         // 세션오류
    dfLOGIN_STATUS_OK = 1,           // 성공
    dfLOGIN_STATUS_GAME = 2,         // 게임중
    dfLOGIN_STATUS_ACCOUNT_MISS = 3, // account 테이블에 AccountNo 없음
    dfLOGIN_STATUS_SESSION_MISS = 4, // Session 테이블에 AccountNo 없음
    dfLOGIN_STATUS_STATUS_MISS = 5,  // Status 테이블에 AccountNo 없음
    dfLOGIN_STATUS_NOSERVER = 6,     // 서비스중인 서버가 없음.
};
BYTE CTestServer::WaitDB(INT64 AccountNo, const WCHAR *const SessionKey, WCHAR *ID, WCHAR *Nick)
{
    //일단은 성공만 반환 나중에 db에서 가져와서 확인.

    //SELECT 컬럼명 FROM 테이블명 WHERE 조건
    CDB::ResultSet result = db.Query("select sessionkey from sessionkey where accountno = %lld", AccountNo);
    if (result.Sucess() == false)
    {
        printf("\tQuery Error %s \n", result.Error().c_str());
        __debugbreak();
    }
    // select의 경우
    for (const auto &row : result)
    {
        std::string token = row["sessionkey"].AsString();

        std::string key, value;
        char SessionKeyA[66]; 
        memcpy(SessionKeyA, SessionKey, 64);
        SessionKeyA[64] = '\0';
        SessionKeyA[65] = '\0';
        key = std::to_string(AccountNo);
        {
            size_t i;
  
            value = SessionKeyA;
            int ttl_ms = 50000;
            InterlockedIncrement(&cnt);
            client->psetex(key, ttl_ms, value);
            client->sync_commit();
        }
        return dfLOGIN_STATUS_OK;
    }

    return dfLOGIN_STATUS_SESSION_MISS;
}

void CTestServer::MonitorThread()
{
    while (1)
    {

        HANDLE hWaitHandle = {m_ServerOffEvent};

        LONG64 UpdateTPS;
        LONG64 before_UpdateTPS = 0;

        LONG64 RecvTPS;
        LONG64 before_RecvTPS = 0;

        std::vector<LONG64> before_arrTPS(m_WorkThreadCnt + 1, 0);
        std::vector<LONG64> Send_arrTPS(m_WorkThreadCnt + 1, 0);

        LONG64 RecvMsgArr[en_PACKET_CS_CHAT__Max]{
            0,
        };
        LONG64 before_RecvMsgArr[en_PACKET_CS_CHAT__Max]{
            0,
        };

        // 얘는 signal 말고 전역변수로 끄자.

        timeBeginPeriod(1);

        DWORD currentTime;
        DWORD nextTime; // 내가 목표로하는 이상적인 시간.

        size_t MaxSessions = sessions_vec.size();

        char ProfilerFormat[2][30] = {
            "Profiler_Mode : Off\n",
            "Profiler_Mode : On\n"};

        // PDH 쿼리 핸들 생성
        PDH_HQUERY hQuery;
        PdhOpenQuery(NULL, NULL, &hQuery);

        PDH_HCOUNTER Process_PrivateByte;
        PdhAddCounter(hQuery, L"\\Process(LoginServer_Project)\\Private Bytes", NULL, &Process_PrivateByte);

        PDH_HCOUNTER Process_NonpagedByte;
        PdhAddCounter(hQuery, L"\\Process(LoginServer_Project)\\Pool Nonpaged Bytes", NULL, &Process_NonpagedByte);

        PDH_HCOUNTER Available_Byte;
        PdhAddCounter(hQuery, L"\\Memory\\Available MBytes", NULL, &Available_Byte);

        PDH_HCOUNTER Nonpaged_Byte;
        PdhAddCounter(hQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &Nonpaged_Byte);

        PDH_HCOUNTER hBytesRecv, hBytesSent, hBytesTotal;
        PDH_HCOUNTER hBytesRecv1, hBytesSent1, hBytesTotal1;
        PDH_HCOUNTER hBytesRecv2, hBytesSent2, hBytesTotal2;

        PDH_HCOUNTER hTcp4Retrans, hTcp4SegSent, hTcp4SegRecv;

        PdhAddCounter(hQuery, (L"\\Network Interface(Realtek PCIe GbE Family Controller)\\Bytes Received/sec"), 0, &hBytesRecv);
        PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection)\\Bytes Received/sec"), 0, &hBytesRecv1);
        PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection _2)\\Bytes Received/sec"), 0, &hBytesRecv2);

        PdhAddCounter(hQuery, (L"\\Network Interface(Realtek PCIe GbE Family Controller)\\Bytes Sent/sec"), 0, &hBytesSent);
        PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection)\\Bytes Sent/sec"), 0, &hBytesSent1);
        PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection _2)\\Bytes Sent/sec"), 0, &hBytesSent2);

        PdhAddCounter(hQuery, (L"\\Network Interface(Realtek PCIe GbE Family Controller)\\Bytes Total/sec"), 0, &hBytesTotal);
        PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection)\\Bytes Total/sec"), 0, &hBytesTotal1);
        PdhAddCounter(hQuery, (L"\\Network Interface(Intel[R] I210 Gigabit Network Connection _2)\\Bytes Total/sec"), 0, &hBytesTotal2);

        PdhAddCounter(hQuery, L"\\TCPv4\\Segments Retransmitted/sec", 0, &hTcp4Retrans);
        PdhAddCounter(hQuery, L"\\TCPv4\\Segments Sent/sec", 0, &hTcp4SegSent);
        PdhAddCounter(hQuery, L"\\TCPv4\\Segments Received/sec", 0, &hTcp4SegRecv);

        PdhCollectQueryData(hQuery);
        CCpuUsage CPUTime;

        {
            PDH_FMT_COUNTERVALUE Process_PrivateByteVal;
            PDH_FMT_COUNTERVALUE Process_Nonpaged_ByteVal;
            PDH_FMT_COUNTERVALUE Available_Byte_ByteVal;
            PDH_FMT_COUNTERVALUE Nonpaged_Byte_ByteVal;

            PDH_FMT_COUNTERVALUE recvVal[3];
            PDH_FMT_COUNTERVALUE sentVal[3];
            PDH_FMT_COUNTERVALUE totalVal[3];
            PDH_FMT_COUNTERVALUE vTcp4Retr, vTcp4Sent, vTcp4Recv;

            currentTime = timeGetTime();
            nextTime; // 내가 목표로하는 이상적인 시간.
            nextTime = currentTime;
            LONG64 TotalTPS = m_UpdateTPS;
            
            InterlockedExchange64(&m_UpdateTPS, 0);

            while (bMonitorThreadOn)
            {
                TotalTPS = InterlockedExchange64(&m_UpdateTPS, 0);
                nextTime += 1000;
    
                {
                    // 1초마다 갱신
                    PdhCollectQueryData(hQuery);

                    CPUTime.UpdateCpuTime();

                    wprintf(L" ============================================ CPU Useage ============================================ \n");

                    wprintf(L" [ Total ]T:%03.2f U : %03.2f  K : %03.2f \t", CPUTime.ProcessorTotal(), CPUTime.ProcessorKernel(), CPUTime.ProcessorUser());
                    wprintf(L" [ Process ] T:%03.2f U : %03.2f  K : %03.2f   \n", CPUTime.ProcessTotal(), CPUTime.ProcessKernel(), CPUTime.ProcessUser());
                    wprintf(L"====================================================================================================\n");

                    // 갱신 데이터 얻음
                    // PDH_FMT_COUNTERVALUE counterVal;
                    PdhGetFormattedCounterValue(Process_PrivateByte, PDH_FMT_LARGE, NULL, &Process_PrivateByteVal);
                    wprintf(L"Process_PrivateByte : %lld Byte\n", Process_PrivateByteVal.largeValue);

                    PdhGetFormattedCounterValue(Process_NonpagedByte, PDH_FMT_LARGE, NULL, &Process_Nonpaged_ByteVal);
                    wprintf(L"Process_Nonpaged_Byte :  %lld Byte\n", Process_Nonpaged_ByteVal.largeValue);

                    PdhGetFormattedCounterValue(Available_Byte, PDH_FMT_LARGE, NULL, &Available_Byte_ByteVal);
                    wprintf(L"Available_Byte :  %lld Byte\n", Available_Byte_ByteVal.largeValue);

                    PdhGetFormattedCounterValue(Nonpaged_Byte, PDH_FMT_LARGE, NULL, &Nonpaged_Byte_ByteVal);
                    wprintf(L"Nonpaged_Byte_ByteVal : %lld Byte\n", Nonpaged_Byte_ByteVal.largeValue);
                }
                // NetWork
                {
                    PdhGetFormattedCounterValue(hBytesRecv, PDH_FMT_DOUBLE, NULL, &recvVal[0]);
                    PdhGetFormattedCounterValue(hBytesRecv1, PDH_FMT_DOUBLE, NULL, &recvVal[1]);
                    PdhGetFormattedCounterValue(hBytesRecv2, PDH_FMT_DOUBLE, NULL, &recvVal[2]);

                    PdhGetFormattedCounterValue(hBytesSent, PDH_FMT_DOUBLE, NULL, &sentVal[0]);
                    PdhGetFormattedCounterValue(hBytesSent1, PDH_FMT_DOUBLE, NULL, &sentVal[1]);
                    PdhGetFormattedCounterValue(hBytesSent2, PDH_FMT_DOUBLE, NULL, &sentVal[2]);

                    PdhGetFormattedCounterValue(hBytesTotal, PDH_FMT_DOUBLE, NULL, &totalVal[0]);
                    PdhGetFormattedCounterValue(hBytesTotal1, PDH_FMT_DOUBLE, NULL, &totalVal[1]);
                    PdhGetFormattedCounterValue(hBytesTotal2, PDH_FMT_DOUBLE, NULL, &totalVal[2]);

                    PdhGetFormattedCounterValue(hTcp4Retrans, PDH_FMT_DOUBLE, NULL, &vTcp4Retr);
                    PdhGetFormattedCounterValue(hTcp4SegSent, PDH_FMT_DOUBLE, NULL, &vTcp4Sent);
                    PdhGetFormattedCounterValue(hTcp4SegRecv, PDH_FMT_DOUBLE, NULL, &vTcp4Recv);

                    double tcp4RetrRatio = 0.0;
                    if (vTcp4Sent.doubleValue > 0.0)
                        tcp4RetrRatio = (vTcp4Retr.doubleValue / vTcp4Sent.doubleValue) * 100.0;

                    wprintf(L"\n ============================================ Network Usage (Bytes/sec) ============================================ \n");
                    wprintf(L"%20s %15s %15s %15s\n",
                            L"Adapter",
                            L"Recv(B/s)",
                            L"Sent(B/s)",
                            L"Total(B/s)");
                    wprintf(L"--------------------------------------------------------------------------\n");

                    wprintf(L"%-40s %15.0f %15.0f %15.0f\n",
                            L"Realtek PCIe GbE Family Controller",
                            recvVal[0].doubleValue,
                            sentVal[0].doubleValue,
                            totalVal[0].doubleValue);

                    wprintf(L"%-40s %15.0f %15.0f %15.0f\n",
                            L"Intel[R] I210 Gigabit Network Connection",
                            recvVal[1].doubleValue,
                            sentVal[1].doubleValue,
                            totalVal[1].doubleValue);

                    wprintf(L"%-40s %15.0f %15.0f %15.0f\n",
                            L"Intel[R] I210 Gigabit Network Connection _2",
                            recvVal[2].doubleValue,
                            sentVal[2].doubleValue,
                            totalVal[2].doubleValue);

                    wprintf(L"\n ============================================ TCP Retransmission ============================================ \n");
                    wprintf(L"TCPv4 Segments Sent/sec          : %.2f\n", vTcp4Sent.doubleValue);
                    wprintf(L"TCPv4 Segments Retransmitted/sec : %.2f\n", vTcp4Retr.doubleValue);
                    wprintf(L"TCPv4 Retrans Ratio              : %.2f %%\n", tcp4RetrRatio);

                    wprintf(L" ============================================================================================================== \n");
                }
                currentTime = timeGetTime();

                time_t currenttt;
                time(&currenttt);
                // MonitorData
                {
                    // InterlockedExchange((DWORD *)&g_MonitorData[enMonitorType::TimeStamp], currenttt);
                    g_MonitorData[enMonitorType::TimeStamp] = (int)currenttt;
                    g_MonitorData[enMonitorType::On] = bOn;
                    g_MonitorData[enMonitorType::Cpu] = (int)CPUTime.ProcessTotal();
                    g_MonitorData[enMonitorType::Memory] = (int)(Process_PrivateByteVal.largeValue / 1024 / 1024);
                    g_MonitorData[enMonitorType::SessionCnt] = (int)GetSessionCount();
       
                    g_MonitorData[enMonitorType::TotalSendTps] = (int)TotalTPS;
                    g_MonitorData[enMonitorType::TotalPackPool_Cnt] = (int)stTlsObjectPool<CMessage>::instance.m_TotalCount;


                    SetEvent(g_hMonitorEvent);
                }

                printf(" ====================================================================== \n");
                printf("%20s : %10lld\n", "DBQuery_RemainCount", m_DBMessageCnt);
                printf("%20s : %10lld\n", "Total Account", cnt);
                printf("%20s : %10lld\n", "DisConnectConunt", iDisCounnectCount);
                printf("%20s : %10lld\n", "SessionID_hash.size", SessionID_hash.size());
                printf("%20s : %10d\n", "player_pool.ActiveCnt", player_pool.m_AllocatedCount);
                printf("%20s : %10d\n", "dbOverlapped_pool.iNodeCnt", dbOverlapped_pool.iNodeCnt);
                printf(" ======================================================================\n ");

                if (nextTime > currentTime)
                    Sleep(nextTime - currentTime);
            }
        }


    }
}

void CTestServer::HeartBeatThread()
{
    DWORD retval;

    while (1)
    {
        retval = WaitForSingleObject(m_ServerOffEvent, 20);
        if (retval != WAIT_TIMEOUT)
            return;
        // server->Update();
    }
}

void CTestServer::DBworkerThread()
{
    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    stDBOverlapped *dbOverlapped;
    cpp_redis::client a;
    client = &a;

    client->connect(RedisIpAddress, 6379);

    db.Connect(AccountDB_IPAddress, DBuser, password, schema, DBPort);

    while (1)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        GetQueuedCompletionStatus(m_hDBIOCP, &transferred, &key, &overlapped, INFINITE);
        _interlockeddecrement64(&m_DBMessageCnt);
        dbOverlapped = reinterpret_cast<stDBOverlapped *>(overlapped);
        if (overlapped == nullptr)
            __debugbreak();

        // TODO : DB_VerifySession의 반환값에 따른 동작이 이게 맞나?
        DB_VerifySession(dbOverlapped->msg);
        dbOverlapped_pool.Release(dbOverlapped);
        _interlockedincrement64(&m_UpdateTPS);
    }

}


void CTestServer::DB_VerifySession( CMessage *msg)
{
    INT64 AccountNo;
    ull SessionID;


    WCHAR SessionKey[32];
    BYTE retval;
    WCHAR ID[20];      // 사용자 ID		. null 포함
    WCHAR Nickname[20]; // 사용자 닉네임	. null 포함

    *msg >> AccountNo;
    *msg >> SessionID;

    msg->GetData(SessionKey, 64);

    retval = WaitDB(AccountNo, SessionKey, ID,Nickname);
    if (retval != dfLOGIN_STATUS_OK)
    {
        CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld ",
                                       L"DB_VerifySession", L"WaitDB retval !=  1",
                                       retval);
        Disconnect(SessionID);
        return;
    }
    {
        //std::shared_lock lock(SessionID_hash_Lock);
        std::shared_lock<SharedMutex> lock(SessionID_hash_Lock);
        auto iter = SessionID_hash.find(SessionID);
        if (iter != SessionID_hash.end())
        {
            if (iter->second->m_ipAddress.compare("10.0.1.2") == 0)
            {
                RES_LOGIN(SessionID, msg, AccountNo, retval, ID, Nickname, GameServerIP, GameServerPort, Dummy1_ChatServerIP, ChatServerPort);
                return;
            }
            else if (iter->second->m_ipAddress.compare("10.0.2.2") == 0)
            {
                RES_LOGIN(SessionID, msg, AccountNo, retval, ID, Nickname, GameServerIP, GameServerPort, Dummy2_ChatServerIP, ChatServerPort);
                return;
            }
        }
        RES_LOGIN(SessionID, msg, AccountNo, retval, ID, Nickname, GameServerIP, GameServerPort, ChatServerIP, ChatServerPort);
        
    }

}

void CTestServer::REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, WORD wVectorLen)
{
    // NetworkThread가 진입.
    CPlayer *player;
    {
        std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);

        if (SessionID_hash.find(SessionID) == SessionID_hash.end())
            return;
        auto iter = Account_hash.find(AccountNo);
        if (iter != Account_hash.end())
        {
            CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld ",
                                           L"REQ_LOGIN", L"AccountNo Already exists ",
                                           SessionID);
            Disconnect(iter->second->m_sessionID);

        
            /*  
            Disconnect(SessionID);
            return;*/
        }
        player = SessionID_hash[SessionID];
        //Account_Hash 추가
        Account_hash[AccountNo] = player;

        // 이미 LoginReq를 보낸적이 있다면,
        if (InterlockedCompareExchange((ull *)&player->m_State, (ull)en_State::LoginWait, (ull)en_State::None) != (ull)en_State::None)
        {
            CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld ",
                                           L"REQ_LOGIN", L"Already Send Login Packet",
                                           SessionID);

            Disconnect(SessionID);
            return;
        }
        player->m_AccountNo = AccountNo;
        // TODO : dbOverlapped_pool.pool 모니터링 필요
        stDBOverlapped *overlapped = reinterpret_cast<stDBOverlapped *>(dbOverlapped_pool.Alloc());
        ZeroMemory(overlapped, sizeof(stDBOverlapped));
        overlapped->_mode = Job_Type::Post;

        msg->InitMessage();
        *msg << AccountNo;
        *msg << SessionID;
        msg->PutData(SessionKey, 64);
        overlapped->msg = msg;

        PostQueuedCompletionStatus(m_hDBIOCP, 0, 0, (LPOVERLAPPED)overlapped);
        _interlockedincrement64(&m_DBMessageCnt);
    }
}

CTestServer::CTestServer(DWORD ContentsThreadCnt, int iEncording, int reduceThreadCount)
    : CLanServer(iEncording), m_ContentsThreadCnt(ContentsThreadCnt)
{
    mysql_library_init(0, nullptr, nullptr);

    int DBThreadCnt;
    {
        WCHAR DBIPAddress[16];
        WCHAR RedisIp[16];
        WCHAR DBId[100];
        WCHAR DBPassword[100];
        WCHAR DBName[100];


        Parser parser;
        parser.LoadFile(L"Config.txt");

        parser.GetValue(L"DBThreadCnt", DBThreadCnt);
        parser.GetValue(L"DBPort", DBPort);

        parser.GetValue(L"DBIPAddress", DBIPAddress, IP_LEN);
        parser.GetValue(L"DBId", DBId, ID_LEN);
        parser.GetValue(L"DBPassword", DBPassword, Password_LEN);
        parser.GetValue(L"DBName", DBName, DBName_LEN);

        parser.GetValue(L"RedisIp", RedisIp, IP_LEN);



        size_t i;
        wcstombs_s(&i, AccountDB_IPAddress, IP_LEN, DBIPAddress, IP_LEN );
        wcstombs_s(&i, RedisIpAddress, IP_LEN, RedisIp, IP_LEN);

        wcstombs_s(&i, DBuser, ID_LEN, DBId, ID_LEN );
        wcstombs_s(&i, password, Password_LEN, DBPassword, Password_LEN );
        wcstombs_s(&i, schema, schema_LEN, DBName, schema_LEN );
  
    }

    // DBConcurrent Thread 세팅
    m_hDBIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, m_lProcessCnt - reduceThreadCount);

    HRESULT hr;
    //player_pool.Initalize(m_maxPlayers);
    //player_pool.Limite_Lock(); // Pool이 더 이상 늘어나지않음.

    // BalanceThread를 위한 정보 초기화.
    {
        m_ServerOffEvent = CreateEvent(nullptr, true, false, nullptr);
        m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    }
    // ContentsThread의 생성
    hHeartBeatThread = WinThread(&CTestServer::HeartBeatThread, this);
    hr = SetThreadDescription(hHeartBeatThread.native_handle(), L"HeartBeatThread");
    RT_ASSERT(!FAILED(hr));

    hMonitorThread =  WinThread(&CTestServer::MonitorThread, this);
    hr = SetThreadDescription(hMonitorThread.native_handle(), L"MonitorThread");
    RT_ASSERT(!FAILED(hr));
    hDBThread_vec.resize(DBThreadCnt);

    for (DWORD i = 0; i < DBThreadCnt; i++)
    {
        std::wstring ContentsThreadName = L"\tDBThread" + std::to_wstring(i);

        hDBThread_vec[i] =  WinThread(&CTestServer::DBworkerThread, this);

        RT_ASSERT(hDBThread_vec[i].native_handle() != nullptr);

        hr = SetThreadDescription(hDBThread_vec[i].native_handle(), ContentsThreadName.c_str());
        RT_ASSERT(!FAILED(hr));
    }

}

CTestServer::~CTestServer()
{

    hHeartBeatThread.join();
    for (DWORD i = 0; i < hDBThread_vec.size(); i++)
    {
        hDBThread_vec[i].join();
    }


}
BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    BOOL retval;
    HRESULT hr;

    m_maxSessions = maxSessions;

    retval = CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);

    //hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);
    //RT_ASSERT(hMonitorThread != nullptr);
    //hr = SetThreadDescription(hMonitorThread, L"\tMonitorThread");
    //RT_ASSERT(!FAILED(hr));

    return retval;
}

void CTestServer::OnRecv(ull SessionID, CMessage *msg)
{
    WORD type;
    *msg >> type;
    PacketProc(SessionID,msg,type);
}

bool CTestServer::OnAccept(ull SessionID, SOCKADDR_IN &addr)
{
    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);

    if (SessionID_hash.find(SessionID) != SessionID_hash.end())
    {
        CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld ",
                                       L"OnAccept", L"SessionID != SessionID_hash.end()",
                                       SessionID);
        return false;
    }
    CPlayer *player = reinterpret_cast<CPlayer*>(player_pool.Alloc());
    if (player == nullptr)
    {
        CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld ",
                                       L"OnAccept", L"Player == nullptr",
                                       SessionID);
        return false;
    }

    char ipAddress[16];
    USHORT port;
    {
        sockaddr_in *ipv4 = (sockaddr_in *)&addr;
        inet_ntop(AF_INET, &ipv4->sin_addr, ipAddress, sizeof(ipAddress));
        port = ntohs(ipv4->sin_port);
    }

    //TODO : 이부분 Flush 되나?
    SessionID_hash[SessionID] = player;
    InterlockedExchange((ull *)&player->m_State, (ull)en_State::None);
    //player->m_State = en_State::None;
    player->m_Timer = timeGetTime();
    player->m_sessionID = SessionID;

    player->m_ipAddress = ipAddress;
    player->m_port = port;

    return true;
}

void CTestServer::OnRelease(ull SessionID)
{
    CPlayer *player;

    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);

    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        __debugbreak();
    player = SessionID_hash[SessionID];

    SessionID_hash.erase(SessionID);
    auto iter = Account_hash.find(player->m_AccountNo);

    if (iter != Account_hash.end())
    {
        if (iter->second == player)
            Account_hash.erase(player->m_AccountNo);
    }

    player_pool.Release(player);

}

void CTestServer::Update()
{
    std::shared_lock<SharedMutex> sessionHashLock(SessionID_hash_Lock);
    
    DWORD currentTime = timeGetTime();
    DWORD distance;

    for (auto& element : SessionID_hash)
    {
        DWORD targetTime = element.second->m_Timer;
        if (currentTime < targetTime)
        {
            continue;
        }
        distance = currentTime - targetTime;
        if (distance > 10000)
        {
            Disconnect(element.first);
        }
    }

}
