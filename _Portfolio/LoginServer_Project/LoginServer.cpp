#include "LoginServer.h"
#include <Windows.h>
#include <timeapi.h>


#include "../../_4Course/_lib/CDB/CDB.h"

#include <cpp_redis/cpp_redis>
#include <iostream>
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")
#pragma comment(lib, "ws2_32.lib")


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


void CTestServer::DB_VerifySession(ull SessionID, CMessage *msg)
{
    INT64 AccountNo;
    WCHAR SessionKey[32];
    BYTE retval;
    WCHAR ID[20];      // 사용자 ID		. null 포함
    WCHAR Nickname[20]; // 사용자 닉네임	. null 포함

    *msg >> AccountNo;
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
void MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    while (1)
    {
        Sleep(1000);

        printf("======================================================================");
        printf("%20s : %10lld\n", "DBQuery_RemainCount", server->m_DBMessageCnt);
        printf("%20s : %10lld\n", "Total Account", cnt);
        printf("%20s : %10lld\n", "DisConnectConunt", server->iDisCounnectCount);
        printf("%20s : %10lld\n", "SessionID_hash.size", server->SessionID_hash.size());
        printf("%20s : %10d\n", "player_pool.iNodeCnt", server->player_pool.iNodeCnt);
        printf("%20s : %10d\n", "dbOverlapped_pool.iNodeCnt", server->dbOverlapped_pool.iNodeCnt);
        printf("======================================================================");

    }
}

void HeartBeatThread(void *arg) 
{
    MyMutexManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo); 

    DWORD retval;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);


    while (1)
    {
        retval = WaitForSingleObject(server->m_ServerOffEvent, 20);
        if (retval != WAIT_TIMEOUT)
            return;
        //server->Update();

    }
    
}

void DBworkerThread(void *arg)
{
    MyMutexManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo); 

    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    stDBOverlapped *dbOverlapped;
    cpp_redis::client a;
    client = &a;

    client->connect(server->RedisIpAddress, 6379);

    db.Connect(server->AccountDB_IPAddress,server->DBuser,server->password,server->schema,server->DBPort);
    
    while (1)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        GetQueuedCompletionStatus(server->m_hDBIOCP, &transferred, &key, &overlapped, INFINITE);
        _interlockeddecrement64(&server->m_DBMessageCnt);
        dbOverlapped = reinterpret_cast<stDBOverlapped *>(overlapped);
        if (overlapped == nullptr)
            __debugbreak();

        // TODO : DB_VerifySession의 반환값에 따른 동작이 이게 맞나?
        server->DB_VerifySession(dbOverlapped->SessionID,dbOverlapped->msg);
        server->dbOverlapped_pool.Release(dbOverlapped);
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

        msg->InitMessage();
        *msg << AccountNo;
        msg->PutData(SessionKey, 64);
        overlapped->msg = msg;
        overlapped->SessionID = SessionID;

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
    hHeartBeatThread = std::move(std::thread(HeartBeatThread, this));
    hr = SetThreadDescription(hHeartBeatThread.native_handle(), L"HeartBeatThread");
    RT_ASSERT(!FAILED(hr));

    hMonitorThread = std::move(std::thread(MonitorThread, this));
    hr = SetThreadDescription(hMonitorThread.native_handle(), L"MonitorThread");
    RT_ASSERT(!FAILED(hr));
    hDBThread_vec.resize(DBThreadCnt);

    for (DWORD i = 0; i < DBThreadCnt; i++)
    {
        std::wstring ContentsThreadName = L"\tDBThread" + std::to_wstring(i);

        hDBThread_vec[i] = std::move(std::thread(DBworkerThread, this));

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

void CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ)
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
