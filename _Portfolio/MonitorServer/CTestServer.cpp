#include "CTestServer.h"
#include <timeapi.h>
#include "../../_4Course/_lib/CDB/CDB.h"

static void MakeYYYYMM_FromEpochSec(int epochSec, char outYYYYMM[7])
{
    time_t tt = (time_t)epochSec;
    tm tmLocal{};
#ifdef _WIN32
    localtime_s(&tmLocal, &tt);
#else
    localtime_r(&tt, &tmLocal);
#endif
    // YYYYMM (6 chars + '\0')
    // tm_year: 1900부터, tm_mon: 0~11
    sprintf_s(outYYYYMM, 7, "%04d%02d", tmLocal.tm_year + 1900, tmLocal.tm_mon + 1);
}


thread_local CDB db;

//TODO : 하드코딩 파써로 바꾸기.
static const std::string gLoginKey = "ajfw@!cv980dSZ[fje#@fdj123948djf";
static const std::string LanIP[3] =
    {
        "127.0.0.1",
        "10.0.1.2",
        "10.0.2.2",
};

CTestServer::CTestServer(bool EnCoding)
    : CLanServer(EnCoding),
      _port(0),
      _ZeroCopy(0), _WorkerCreateCnt(0), _reduceThreadCount(0), _noDelay(0), _MaxSessions(0)
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

 
        parser.GetValue(L"DBPort", DBPort);

        parser.GetValue(L"DBIPAddress", DBIPAddress, IP_LEN);
        parser.GetValue(L"DBId", DBId, ID_LEN);
        parser.GetValue(L"DBPassword", DBPassword, Password_LEN);
        parser.GetValue(L"DBName", DBName, DBName_LEN);


        size_t i;
        wcstombs_s(&i, LogDB_IPAddress, IP_LEN, DBIPAddress, IP_LEN);

        wcstombs_s(&i, DBuser, ID_LEN, DBId, ID_LEN);
        wcstombs_s(&i, password, Password_LEN, DBPassword, Password_LEN);
        wcstombs_s(&i, schema, schema_LEN, DBName, schema_LEN);

        m_hDBIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, 1);

    }

    _hDBWorkerThread = WinThread(&CTestServer::DBWorkerThread, this);
    _hDBTimerThread = WinThread(&CTestServer::DBTimerThread, this);
    _hMonitorThread = WinThread(&CTestServer::MonitorThread, this);
}

void CTestServer::DBWorkerThread()
{
    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    stDBOverlapped *dbOverlapped;

    db.Connect(LogDB_IPAddress, DBuser, password, schema, DBPort);

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

        Win32::AtomicDecrement(m_DBMessageCnt);

        dbOverlapped = reinterpret_cast<stDBOverlapped *>(overlapped);
        if (overlapped == nullptr)
            __debugbreak();
        switch (dbOverlapped->_mode)
        {
        case Job_Type::Post:
            HandleDBLogPost(dbOverlapped->msg);
            break;
        case Job_Type::DBStore:
            HandleDBLogInsert(dbOverlapped->msg);
            break;
        }
        // TODO : DB_VerifySession의 반환값에 따른 동작이 이게 맞나?

        dbOverlapped_pool.Release(dbOverlapped);
    }
}

void CTestServer::DBTimerThread()
{
    while (1)
    {
        Sleep(5000);
        DB_StoreRequest();
    }
}



void CTestServer::DB_StoreRequest()
{
    //특정 쓰레드에서 해당 메세지를 생성후 dbIOCP에 Enq
    stDBOverlapped *overlapped = (stDBOverlapped*)dbOverlapped_pool.Alloc();
    overlapped->_mode = Job_Type::DBStore;
    overlapped->msg = nullptr;

    PostQueuedCompletionStatus(m_hDBIOCP, 0, NULL, overlapped);
}

void CTestServer::HandleDBLogInsert(CMessage *msg)
{
    if (msg != nullptr)
        __debugbreak(); // Timer가 nullptr로 보내는 게 정상

    // DBWorkerThread 단일이라면, 여기서 _dbinfoSet_hash 접근은 락 없이도 OK
    // (Post도 동일 쓰레드에서 HandleDBLogPost로만 push하니까)

    constexpr int kMaxRowsPerInsert = 500; // 적당히 (너무 크면 max_allowed_packet 영향)

    // ServerNo별로 쌓인 list를 로컬로 빼서 처리
    for (auto &kv : _dbinfoSet_hash)
    {
        const BYTE serverNo = kv.first;
        stDBinfoSet &set = kv.second;

        if (set.infos.empty())
            continue;

        // 1) 빠르게 로컬로 이동 (원본 비움)
        std::list<stDBinfoSet::stDBinfo> local;
        local.splice(local.end(), set.infos); // set.infos는 비워짐

        // 2) 월별(YYYYMM)로 그룹핑 (5초 flush면 보통 한 달 안에서만 움직이지만 안전하게)
        //    간단히: 순회하며 현재 월이 바뀌면 flush
        char curYYYYMM[7] = {0};
        char yyyymm[7] = {0};

        std::string sql;
        sql.reserve(64 * 1024);

        int rowsInCurrentInsert = 0;
        bool hasOpenInsert = false;

        auto flushCurrentInsert = [&]()
        {
            if (!hasOpenInsert)
                return;
            sql.push_back(';');

            CDB::ResultSet r = db.Query("%s", sql.c_str());
            if (!r.Sucess())
            {
                printf("Insert Error: %s\nSQL: %s\n", r.Error().c_str(), sql.c_str());
                __debugbreak();
            }

            sql.clear();
            rowsInCurrentInsert = 0;
            hasOpenInsert = false;
        };

        for (auto &info : local)
        {
            MakeYYYYMM_FromEpochSec(info.TimeStamp, yyyymm);

            // 월이 바뀌면 이전 INSERT flush
            if (curYYYYMM[0] == '\0')
            {
                strcpy_s(curYYYYMM, yyyymm);
                EnsureMonthlyLogTable(curYYYYMM);
            }
            else if (strcmp(curYYYYMM, yyyymm) != 0)
            {
                flushCurrentInsert();
                strcpy_s(curYYYYMM, yyyymm);
                EnsureMonthlyLogTable(curYYYYMM);
            }

            // 새로운 INSERT 시작
            if (!hasOpenInsert)
            {
                // auto_increment no는 빼고, 컬럼을 명시하는 게 안전
                // logtime은 epoch seconds -> FROM_UNIXTIME 사용
                char head[256];
                sprintf_s(head, sizeof(head),
                          "INSERT INTO `logdb`.`monitorlog_%s` (`logtime`,`serverno`,`type`,`avg`,`min`,`max`) VALUES ",
                          curYYYYMM);
                sql.append(head);
                hasOpenInsert = true;
            }
            else
            {
                sql.push_back(',');
            }

            // 지금은 단일 값만 있으니 avg=min=max=DataValue로 저장
            // (나중에 5분 집계 후 저장 구조로 확장 가능)
            char row[256];
            sprintf_s(row, sizeof(row),
                      "(FROM_UNIXTIME(%d),%u,%u,%d,%d,%d)",
                      info.TimeStamp,
                      (unsigned)serverNo,
                      (unsigned)info.DataType,
                      info.DataValue, info.DataValue, info.DataValue);
            sql.append(row);

            rowsInCurrentInsert++;
            if (rowsInCurrentInsert >= kMaxRowsPerInsert)
            {
                flushCurrentInsert();
            }
        }

        // 남은 것 flush
        flushCurrentInsert();
    }
}


void CTestServer::HandleDBLogPost(CMessage *msg)
{
    BYTE ServerNo;
    BYTE DataType;
    int DataValue;
    int TimeStamp;

    *msg >> ServerNo;
    *msg >> DataType;
    *msg >> DataValue;
    *msg >> TimeStamp;


     stDBinfoSet& infoSet = _dbinfoSet_hash[ServerNo];
    infoSet.infos.emplace_back(DataType, DataValue, TimeStamp);
    stTlsObjectPool<CMessage>::Release(msg);
}

void CTestServer::DB_LogPost(BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp)
{
    CMessage *msg = (CMessage*)stTlsObjectPool<CMessage>::Alloc();
    *msg << ServerNo;
    *msg << DataType;
    *msg << DataValue;
    *msg << TimeStamp;

    stDBOverlapped *overlapped = (stDBOverlapped*)dbOverlapped_pool.Alloc();
    overlapped->_mode = Job_Type::Post;
    overlapped->msg = msg;

    PostQueuedCompletionStatus(m_hDBIOCP, 0, NULL, overlapped);
    Win32::AtomicIncrement(m_DBMessageCnt);

}


// Parser의 기능으로  Address를 정하도록 하기.                                                  
void CTestServer::MonitorThread()
{

    const wchar_t *ConfigFormat[] =
        {
            L"+------------------------------------ Config Info ------------------------------------+\n",
            L"| MonitorServerIP   |  Port | Worker | ReduceConc | ZeroCopy | MaxSession |\n",
            L"+-------------------+-------+--------+------------+----------+------------+\n",
            L"| %-17ls | %5d | %6d | %10d | %8d | %10d |\n",
            L"+-------------------+-------+--------+------------+----------+------------+\n",
        };
    const wchar_t *SessionCountFormat[] =
        {
            L"+---------------------------- Session Info ----------------------------+\n",
            L"| UserCount | WaitLogin |\n",
            L"+-----------+-----------+\n",
            L"| %9d | %9d |\n",
            L"+-----------+-----------+\n",
        };
    const wchar_t *ClientInfoFormat[] =
        {
            L"+---------------------------- Client Info ----------------------------+\n",
            L"| ClientType  | Target IP         |  Port |\n",
            L"+-------------+-------------------+-------+\n",
            L"| %-11ls | %-17ls | %5d |\n",
            L"+-------------+-------------------+-------+\n",
        };

    const wchar_t *ClientTypeFormat[(int)enClientType::Max] =
        {
            L"LoginServer",
            L"ChatServer",
            L"GameServer",
            L"Monitortool", // 다른 프로토콜로 접속
        };

    // 어떤 IP가 나에게 접속하였는지.
    // Client의 타입 과 IP를 모니터링하자.

    DWORD currentTime = timeGetTime();
    DWORD PrintTime = currentTime;
    int frame = 0;
    while (bOn)
    {
        // 프레임 단위로 점프
        frame++;

        currentTime = timeGetTime();

        if (PrintTime <= currentTime)
        {
            // printf("Frame :  %d \n", frame);

            system("cls");
            PrintTime += 1000;
            frame = 0;
            std::shared_lock<SharedMutex> sessionHashLock(SessionID_hash_Lock);
            std::shared_lock<SharedMutex> waitLoginHashLock(waitLogin_hash_Lock);

            // ConfigInfo
            {
                wprintf(ConfigFormat[0]);
                wprintf(ConfigFormat[1]);
                wprintf(ConfigFormat[2]);
                wprintf(ConfigFormat[3], _bindAddr, _port, _WorkerCreateCnt, _reduceThreadCount, _noDelay, _MaxSessions);
                wprintf(ConfigFormat[4]);
            }

            {
                wprintf(SessionCountFormat[0]);
                wprintf(SessionCountFormat[1]);
                wprintf(SessionCountFormat[2]);
                wprintf(SessionCountFormat[3], SessionID_hash.size(), waitLogin_hash.size());
                wprintf(SessionCountFormat[4]);
            }
            wprintf(ClientInfoFormat[0]);
            wprintf(ClientInfoFormat[1]);
            wprintf(ClientInfoFormat[2]);

            for (auto &session : SessionID_hash)
            {
                stPlayer *player = session.second;

                wprintf(ClientInfoFormat[3], ClientTypeFormat[int(player->m_type)], player->m_wipAddress, player->m_port);
                wprintf(ClientInfoFormat[4]);
            }
        }

        if (currentTime < PrintTime)
        {
            Sleep(PrintTime - currentTime);
        }
    }
}

BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    memcpy(_bindAddr, bindAddress, 16);

    _port = port;
    _ZeroCopy = ZeroCopy;
    _WorkerCreateCnt = WorkerCreateCnt;
    _reduceThreadCount = maxConcurrency;
    _noDelay = useNagle;
    _MaxSessions = maxSessions;

    CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);
    return 0;
}

bool CTestServer::OnAccept(ull SessionID, SOCKADDR_IN &addr)
{
    // 하는 일 :
    // SessionID_hash에 추가. Ip ,Port 저장.

    {
        std::shared_lock<SharedMutex> sessionHashLock(SessionID_hash_Lock);
        auto iter = SessionID_hash.find(SessionID);

        if (iter != SessionID_hash.end())
        {
            // 중복 Session ID가 존재하면 안됨.
            CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"%-10s %10s %05lld ",
                                           L"OnAccept", L"SessionID != SessionID_hash.end()",
                                           SessionID);
            return false;
        }
    }

    std::lock_guard<SharedMutex> waitLoginHashLock(waitLogin_hash_Lock);
    auto iter = waitLogin_hash.find(SessionID);

    if (iter != waitLogin_hash.end())
    {
        // 중복 ID가 존재하면 안됨.
        CSystemLog::GetInstance()->Log(L"Contents_DisConnect", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld ",
                                       L"OnAccept", L"SessionID != SessionID_hash.end()",
                                       SessionID);
        return false;
    }

    stPlayer *player = reinterpret_cast<stPlayer *>(player_pool.Alloc());
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

    // TODO : 이부분 Flush 되나?
    waitLogin_hash.insert({SessionID, player});

    player->m_type = enClientType::Max;
    player->m_Timer = timeGetTime();
    player->m_sessionID = SessionID;
    player->m_ServerNo = 0;

    memcpy(player->m_ipAddress, ipAddress, 16);
    MultiByteToWideChar(CP_UTF8, 0, player->m_ipAddress, -1, player->m_wipAddress, sizeof(player->m_wipAddress) / sizeof(wchar_t));



    player->m_port = port;

    return true;
}

void CTestServer::OnRecv(ull SessionID, CMessage *msg)
{
    WORD type;
    *msg >> type;
    if (PacketProc(SessionID, msg, type) == false)
    {
        CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L" PacketProc false return ");
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
    }
}

void CTestServer::OnRelease(ull SessionID)
{
    stPlayer *player;
    // LoginPack이 처리된 세션이 연결이 끊긴 경우.
    {
        std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);
        auto iter = SessionID_hash.find(SessionID);

        if (iter != SessionID_hash.end())
        {
            player = iter->second;

            SessionID_hash.erase(SessionID);
            if (player->m_type == enClientType::ChatServer || player->m_type == enClientType::LoginServer)
            {
                std::lock_guard<SharedMutex> ServerNohashLock(ServerNo_hash_Lock);
                auto iter = ServerNo_hash.find(player->m_ServerNo);
                if (iter != ServerNo_hash.end())
                    ServerNo_hash.erase(iter);

            }
            player_pool.Release(player);
            return;
        }
    }

    // LoginPack이 처리되기전에 연결이 끊긴 경우.
    {
        std::lock_guard<SharedMutex> waitLoginHashLock(waitLogin_hash_Lock);
        auto iter = waitLogin_hash.find(SessionID);

        if (iter != waitLogin_hash.end())
        {
            player = iter->second;

            waitLogin_hash.erase(SessionID);
            player_pool.Release(player);
            return;
        }
    }

    __debugbreak();
}

void CTestServer::REQ_MONITOR_LOGIN(ull SessionID, CMessage *msg, int ServerNo, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    stPlayer *player;

    // 범위 밖이면 끊음
    if (50 <= ServerNo)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L" 50 <= ServerNo ");
        return;
    }
    {
        // WaitLoginHash에 없다면 끊음.
        std::lock_guard<SharedMutex> SessionIDHashLock(SessionID_hash_Lock);
        // 이미 ServerNo가 존재한다면 둘 다 끊음.
        {
            std::lock_guard<SharedMutex> ServerNoHashLock(ServerNo_hash_Lock);
            auto ServerNoIter = ServerNo_hash.find(ServerNo);

            if (ServerNoIter != ServerNo_hash.end())
            {
                // 겹치는 ServerNo면 둘다 끊기
                player = ServerNoIter->second;
                stTlsObjectPool<CMessage>::Release(msg);
                Disconnect(player->m_sessionID);
                Disconnect(SessionID);
                CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L"Same ServerNo");
                return;
            }

            {
                // WaitLoginHash에 없다면 끊음.
                std::lock_guard<SharedMutex> waitLoginHashLock(waitLogin_hash_Lock);

                auto waitLoginiter = waitLogin_hash.find(SessionID);
                if (waitLoginiter == waitLogin_hash.end())
                {
                    stTlsObjectPool<CMessage>::Release(msg);
                    Disconnect(SessionID);
                    CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L"WaitLoginHash Not Found");
                    return;
                }
                player = waitLoginiter->second;
                if (LanIP[0].compare(player->m_ipAddress) == 0 || LanIP[1].compare(player->m_ipAddress) || LanIP[2].compare(player->m_ipAddress) == 0)
                {
                    waitLogin_hash.erase(waitLoginiter);
                    ServerNo_hash.insert({ServerNo, player});
                }
                else
                {
                    // Lan이 아닌 경우 끊기.
                    stTlsObjectPool<CMessage>::Release(msg);
                    CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L"Session Is  Not LAN ");
                    Disconnect(SessionID);
                    return;
                }
            }
        }

        if (ServerNo < 10)
            player->m_type = enClientType::LoginServer;
        else if (ServerNo < 20)
            player->m_type = enClientType::ChatServer;
        else if (ServerNo < 50)
            player->m_type = enClientType::GameServer;

        player->m_ServerNo = ServerNo;

        auto iter = SessionID_hash.find(SessionID);
        if (iter != SessionID_hash.end())
            __debugbreak();
        SessionID_hash.insert({SessionID, player});
    }
}

void CTestServer::REQ_MONITOR_UPDATE(ull SessionID, CMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    // 해당 메세지는 Server에서 보내준 것
    // 완료통지로 모든 Tool에게 보낸다.
    std::shared_lock<SharedMutex> SessionIDHashLock(SessionID_hash_Lock);
    auto iter = SessionID_hash.find(SessionID);
    if (iter == SessionID_hash.end())
    {
        // Login이 오지않았는데 메세지가 옴.
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L"LoginPacket Not Recv");
        return;
    }

    std::vector<ull> sendTarget;
    for (auto &element : SessionID_hash)
    {
        if (element.second->m_type == enClientType::MonitorClient)
            sendTarget.emplace_back(element.first);
    }
    DB_LogPost(iter->second->m_ServerNo, DataType, DataValue, TimeStamp);

    Proxy::RES_MONITOR_UPDATE(SessionID, msg, iter->second->m_ServerNo, DataType, DataValue, TimeStamp, en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE, true, &sendTarget, sendTarget.size());
}

void CTestServer::REQ_MONITOR_TOOL_LOGIN(ull SessionID, CMessage *msg, WCHAR *LoginSessionKey, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    stPlayer *player;

    char Loginkey[33];
    memset(Loginkey, 0, 33);
    memcpy(Loginkey, LoginSessionKey, 32);

    if (gLoginKey.compare(Loginkey) != 0)
    {
        //Proxy::RES_MONITOR_TOOL_LOGIN(SessionID, msg, (BYTE)dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY);
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L" Loginkey false return ");

        return;
    }

    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);
    auto iter = SessionID_hash.find(SessionID);
    if (iter != SessionID_hash.end())
    {
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L" REQ_MONITOR_TOOL_LOGIN SessionID found ");
        return;
    }
    std::lock_guard<SharedMutex> waitLoginHashLock(waitLogin_hash_Lock);

    auto waitLoginiter = waitLogin_hash.find(SessionID);
    if (waitLoginiter == waitLogin_hash.end())
    {
        CSystemLog::GetInstance()->Log(L"MonitorServer_DisConnect", en_LOG_LEVEL::SYSTEM_Mode, L" REQ_MONITOR_TOOL_LOGIN waitLogin_hash not found ");
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }
    player = waitLoginiter->second;
    player->m_type = enClientType::MonitorClient;
    if (LanIP[0].compare(player->m_ipAddress) == 0 || LanIP[1].compare(player->m_ipAddress) || LanIP[2].compare(player->m_ipAddress) == 0)
    {
        waitLogin_hash.erase(waitLoginiter);
        SessionID_hash.insert({SessionID, player});
    }

    Proxy::RES_MONITOR_TOOL_LOGIN(SessionID, msg, (BYTE)dfMONITOR_TOOL_LOGIN_OK);
}























void CTestServer::EnsureMonthlyLogTable(const char *yyyymm)
{
    // monitorlog_YYYYMM 테이블이 없으면 템플릿으로 생성
    // (table name은 파라미터 바인딩 불가 -> 문자열 포맷으로 만들어야 함)
    CDB::ResultSet r = db.Query(
        "CREATE TABLE IF NOT EXISTS `logdb`.`monitorlog_%s` LIKE `logdb`.`monitorlog_template`;",
        yyyymm);
    if (!r.Sucess())
    {
        printf("EnsureMonthlyLogTable Error: %s\n", r.Error().c_str());
        __debugbreak();
    }
}
