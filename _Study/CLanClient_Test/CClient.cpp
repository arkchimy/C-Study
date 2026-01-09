#include "CClient.h"

bool CClient::PackProc(CMessage *msg)
{
    WORD wType;

    *msg >> wType;

    return false;
}

CClient::CClient(int PlayerMaxCnt)
{
    stPlayer *player;
    for (int i = 0; i < PlayerMaxCnt; i++)
    {
        player = new stPlayer();
        playerStack.push(player);
    }
}

void CClient::OnEnterJoinServer(ull SessionID)
{
    stPlayer *player;
    if (playerStack.empty())
        __debugbreak();

    std::lock_guard<std::shared_mutex> lock(m_SessionMap);

    if (SessionID_map.find(SessionID) != SessionID_map.end())
        __debugbreak();

    player = playerStack.top();
    playerStack.pop();
    player->_SessionID = SessionID;
    SessionID_map.emplace(SessionID, player);
}

void CClient::OnLeaveServer(ull SessionID)
{

    std::lock_guard<std::shared_mutex> lock(m_SessionMap);

    stPlayer *player;
    if (SessionID_map.find(SessionID) == SessionID_map.end())
        __debugbreak();
    player = SessionID_map[SessionID];
    SessionID_map.erase(SessionID);

    playerStack.push(player);
}

void CClient::OnRecv(CMessage *msg)
{
    bool ProcRetval;
    ProcRetval = PackProc(msg);
}

void CClient::OnSend(int sendsize)
{
}

void CClient::OnRelease(ull SessionID)
{
    stPlayer *player;
    CMessage *msg;


    msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
    *msg << (unsigned short)en_PACKET_Player_Delete;
    InterlockedExchange(&msg->ownerID, SessionID);

    OnRecv(msg);

}
