#include "CTestServer.h"
#include <timeapi.h>

CTestServer::CTestServer(bool EnCoding)
    : CLanServer(EnCoding)
{
}

bool CTestServer::OnAccept(ull SessionID, SOCKADDR_IN &addr)
{
    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);
    auto iter = SessionID_hash.find(SessionID);

    if (iter != SessionID_hash.end())
    {
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
    SessionID_hash.insert({SessionID, player});

    player->m_type = ClientType::Max;
    player->m_Timer = timeGetTime();
    player->m_sessionID = SessionID;
    player->m_ServerNo = 0;

    memcpy(player->m_ipAddress, ipAddress, 16);

    player->m_port = port;

    return true;
}

void CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ)
{
    WORD type;
    *msg >> type;
    if (PacketProc(SessionID, msg, type) == false)
        Disconnect(SessionID);
}

void CTestServer::OnRelease(ull SessionID)
{
    stPlayer *player;

    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);
    auto iter = SessionID_hash.find(SessionID);

    if (iter == SessionID_hash.end())
        __debugbreak();
    player = iter->second;

    SessionID_hash.erase(SessionID);
    player_pool.Release(player);
}

void CTestServer::REQ_LOGIN_Server(ull SessionID, CMessage *msg, int ServerNo, WORD wType , BYTE bBroadCast , std::vector<ull> *pIDVector , size_t wVectorLen )
{
    stPlayer *player;
    std::lock_guard<SharedMutex> sessionHashLock(SessionID_hash_Lock);
    auto ServerNoIter = ServerNo_hash.find(ServerNo);
    if (ServerNoIter == ServerNo_hash.end())
    {
        Disconnect(SessionID);
        return;
    }

    auto iter = SessionID_hash.find(SessionID);
    if (iter == SessionID_hash.end())
    {
        Disconnect(SessionID);
        return;
    }
    player = iter->second;

    if (ServerNo < 10)
        player->m_type = ClientType::LoginServer;
    else if (ServerNo < 20)
        player->m_type = ClientType::ChatServer;
    else if (ServerNo < 50)
        player->m_type = ClientType::GameServer;
    else
    {
        Disconnect(SessionID);
        return;
    }
    player->m_ServerNo = ServerNo;
}
void CTestServer::REQ_DATA_UPDATE(ull SessionID, CMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType , BYTE bBroadCast , std::vector<ull> *pIDVector , size_t wVectorLen )
{

}
void CTestServer::REQ_LOGIN_Client(ull SessionID, CMessage *msg, WCHAR *LoginSessionKey, WORD wType , BYTE bBroadCast , std::vector<ull> *pIDVector , size_t wVectorLen ) 
{
    
}
void CTestServer::REQ_MONITOR_TOOL_UPDATE(ull SessionID, CMessage *msg, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp, WORD wType , BYTE bBroadCast , std::vector<ull> *pIDVector , size_t wVectorLen ) 
{

}
