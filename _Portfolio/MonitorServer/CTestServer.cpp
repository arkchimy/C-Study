#include "CTestServer.h"
#include <timeapi.h>

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

    _hMonitorThread = WinThread(&CTestServer::MonitorThread, this);
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
        return;
    }

    std::vector<ull> sendTarget;
    for (auto &element : SessionID_hash)
    {
        if (element.second->m_type == enClientType::MonitorClient)
            sendTarget.emplace_back(element.first);
    }
    Proxy::RES_MONITOR_UPDATE(SessionID, msg, 0, DataType, DataValue, TimeStamp, en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE, true, &sendTarget, sendTarget.size());
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
        Proxy::RES_MONITOR_TOOL_LOGIN(SessionID, msg, (BYTE)dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY);
        // DisConnect
        return;
    }

    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);
    auto iter = SessionID_hash.find(SessionID);
    if (iter != SessionID_hash.end())
    {
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }
    std::lock_guard<SharedMutex> waitLoginHashLock(waitLogin_hash_Lock);

    auto waitLoginiter = waitLogin_hash.find(SessionID);
    if (waitLoginiter == waitLogin_hash.end())
    {
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
