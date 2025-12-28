#include "CClient.h"

bool CClient::PackProc(CMessage *msg)
{
    WORD wType;

    *msg >> wType;

    return false;
}


CClient::CClient(int PlayerMaxCnt)
{
    CPlayer *player; 
    for (int i = 0; i < PlayerMaxCnt; i++)
    {
        player = new CPlayer();
        playerStack.push(player);
    }
}

void CClient::OnEnterJoinServer(ull SessionID)
{
    CPlayer *player;
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

    CPlayer *player;
    if (SessionID_map.find(SessionID) == SessionID_map.end())
        __debugbreak();
    player = SessionID_map[SessionID];
    SessionID_map.erase(SessionID);

    playerStack.push(player);
}

void CClient::OnRecv(CMessage * msg)
{
    bool ProcRetval;
    ProcRetval = PackProc(msg);

}

void CClient::OnSend(int sendsize)
{

}

void CClient::OnRelease(ull SessionID)
{

}

