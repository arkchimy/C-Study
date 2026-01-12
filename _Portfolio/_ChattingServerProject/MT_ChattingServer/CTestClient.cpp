#include "CTestClient.h"
#include "MonitorData.h"


CTestClient::CTestClient(bool bEncoding)
    : CLanClient(bEncoding)
{
    _hTimer = WinThread(&CTestClient::TimerThread, this);

}

void CTestClient::TimerThread()
{
    CClientMessage *msg;
    ull SessionID;
    while (1)
    {
        int MonitorData[enMonitorType::Max]{0,};
        WaitForSingleObject(g_hMonitorEvent, INFINITE);
        
        SessionID = session.m_SeqID;

        for (int i = 0; i < enMonitorType::Max; i++)
        {
            MonitorData[i] = g_MonitorData[i];
        }

        for (int i = 1; i < enMonitorType::Max; i++)
        {
            msg = (CClientMessage *)stTlsObjectPool<CClientMessage>::Alloc();
            msg->ownerID = SessionID;

            *msg << en_PACKET_SS_MONITOR_DATA_UPDATE;
            *msg << (BYTE)(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN + i - 1);
            *msg << MonitorData[i];
            *msg << MonitorData[0]; // timeStamp

            PostReQuest_iocp(SessionID,msg);
        }

    }
}


void CTestClient::OnEnterJoinServer(ull SessionID)
{
    //pqcs를 통해 내  IOCP 에 REQ_MONITOR_LOGIN 메세지를 만들어 PQCS 한다.
    CClientMessage* msg =  (CClientMessage*)stTlsObjectPool<CClientMessage>::Alloc();
    // 내가 직접 넣는다고치고 PQCS를 하면 디코드를 하지않을까. 
    // 그러면 Decode를 하지않고, 바로 받아보자. 
    // 보낼때만 Encode하는 방식
    int ServerNo = 11;
    stPlayer *player;

    std::lock_guard<SharedMutex> sessionHashLock(sessiondhash_lock);
    auto iter = sessiondID_Hash.find(SessionID);
    if (iter != sessiondID_Hash.end())
    {
        __debugbreak();
    }

    player = (stPlayer*)player_Pool.Alloc();
    player->m_SeqID = SessionID;

    msg->ownerID = SessionID;

    *msg << en_PACKET_SS_MONITOR_LOGIN;
    *msg << ServerNo;
    
    CSystemLog::GetInstance()->Log(L"ClientError", en_LOG_LEVEL::SYSTEM_Mode, L"OnEnterJoinServer SessionID : %lld", SessionID);
    PostReQuest_iocp(SessionID , msg);

}

void CTestClient::OnLeaveServer()
{
    CSystemLog::GetInstance()->Log(L"ClientError", en_LOG_LEVEL::SYSTEM_Mode, L" Call OnLeaveServer ");
    Parser parser;
    parser.LoadFile(L"Config.txt");

    wchar_t ip[16];
    unsigned short port;

    parser.GetValue(L"MonitorServer_IP_Address", ip, 16);
    parser.GetValue(L"MonitorServer_IP_Port", port);

    CSystemLog::GetInstance()->Log(L"ClientError", en_LOG_LEVEL::SYSTEM_Mode, L" ReConnect ");
    
    stPlayer *player;

   

    std::lock_guard<SharedMutex> sessionHashLock(sessiondhash_lock);
    ull SessionID = session.m_SeqID;

    auto iter = sessiondID_Hash.find(SessionID);
    if (iter == sessiondID_Hash.end())
    {
        __debugbreak();
    }

    player = iter->second;
    sessiondID_Hash.erase(iter);

    player_Pool.Release(player);
    ReConnect(ip,port);

}

void CTestClient::OnRecv(ull SessionID , CClientMessage *msg)
{
    WORD type;
    *msg >> type;
    if (SessionID != msg->ownerID)
    {
        stTlsObjectPool<CClientMessage>::Release(msg);
        CSystemLog::GetInstance()->Log(L"ClientError", en_LOG_LEVEL::SYSTEM_Mode, L" OnRecv PacketProc DisConnect %lld", SessionID);
        return;
    }
    if (PacketProc(SessionID, msg, type) == false)
    {
        stTlsObjectPool<CClientMessage>::Release(msg);
        CSystemLog::GetInstance()->Log(L"ClientError", en_LOG_LEVEL::SYSTEM_Mode, L" OnRecv PacketProc DisConnect %lld", msg->ownerID);
        Disconnect(msg->ownerID);
    }
}

void CTestClient::REQ_MONITOR_LOGIN(ull SessionID, CClientMessage *msg, int ServerNo, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    
    Proxy::RES_MONITOR_LOGIN(SessionID , msg, ServerNo);
}

void CTestClient::REQ_MONITOR_UPDATE(ull SessionID, CClientMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType, BYTE bBroadCast, std::vector<ull> *pIDVector, size_t wVectorLen)
{
    
    Proxy::RES_MONITOR_UPDATE(SessionID,msg, DataType, DataValue, TimeStamp);
}
