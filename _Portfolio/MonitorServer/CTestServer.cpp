#include "CTestServer.h"
#include <timeapi.h>

CTestServer::CTestServer(bool EnCoding)
    : CLanServer(EnCoding)
{
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

    player->m_type = ClientType::Max;
    player->m_Timer = timeGetTime();
    player->m_sessionID = SessionID;
    player->m_ServerNo = 0;

    memcpy(player->m_ipAddress, ipAddress, 16);

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

void CTestServer::REQ_LOGIN_Server(ull SessionID, CMessage *msg, int ServerNo, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    stPlayer *player;

    // 범위 밖이면 끊음
    if (50 <= ServerNo)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);

        return;
    }

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
            waitLogin_hash.erase(waitLoginiter);

            ServerNo_hash.insert({ServerNo, player});
        }
    }



    if (ServerNo < 10)
        player->m_type = ClientType::LoginServer;
    else if (ServerNo < 20)
        player->m_type = ClientType::ChatServer;
    else if (ServerNo < 50)
        player->m_type = ClientType::GameServer;

    player->m_ServerNo = ServerNo;

    {
        // WaitLoginHash에 없다면 끊음.
        std::lock_guard<SharedMutex> SessionIDHashLock(SessionID_hash_Lock);
        auto iter = SessionID_hash.find(SessionID);
        if (iter != SessionID_hash.end())
            __debugbreak();
        SessionID_hash.insert({SessionID, player});
    }

}

void CTestServer::REQ_TOOL_DATA_UPDATE(ull SessionID, CMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    // 해당 메세지는  타이머 쓰레드를 만들어서
    // PQCS를 할예정  데이터를 그대로 전송한다.

    std::shared_lock<SharedMutex> SessionIDHashLock(SessionID_hash_Lock);
    auto iter = SessionID_hash.find(SessionID);
    if (iter == SessionID_hash.end())
    {
        //Login이 오지않았는데 메세지가 옴.
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }

    Proxy::RES_TOOL_DATA_UPDATE(SessionID, msg, DataType, DataValue, TimeStamp);



}
void CTestServer::REQ_LOGIN_Client(ull SessionID, CMessage *msg, WCHAR *LoginSessionKey, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{

}
void CTestServer::REQ_MONITOR_TOOL_UPDATE(ull SessionID, CMessage *msg, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
}
