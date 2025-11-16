#include "stdafx.h"
#include "CTestServer.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include <thread>
#include "../lib/CNetwork_lib/Proxy.h"

extern SRWLOCK srw_Log;
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지
unsigned MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    HANDLE hWaitHandle = {server->m_ServerOffEvent};

    DWORD wait_retval;

    LONG64 *arrTPS;
    LONG64 *before_arrTPS;

    arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    before_arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));
    ZeroMemory(before_arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));

    while (1)
    {
        wait_retval = WaitForSingleObject(hWaitHandle, 1000);

        if (wait_retval != WAIT_OBJECT_0)
        {
            for (int i = 0; i <= server->m_WorkThreadCnt; i++)
            {
                arrTPS[i] = server->arrTPS[i] - before_arrTPS[i];
                before_arrTPS[i] = server->arrTPS[i];
            }

            printf(" ==================================\n ");
            printf("%20s %5lld \n", "Total_Sessions:", server->GetSessionCount());
            printf("%20s %5lld \n", "prePlayerCount:", server->m_prePlayerCount);
            printf("%20s %5lld \n", "Total_Players:", server->GetPlayerCount());
            printf (" ==================================\n ");

            printf(" Total iDisCounnectCount: %llu\n", server->iDisCounnectCount);
            printf(" IdxStack Size: %lld\n", server->Get_IdxStack());
            printf(" ReleaseSessions Size: %lld\n", server->GetReleaseSessions());

            printf(" Accept TPS : %lld\n", arrTPS[0]);

            for (int i = 1; i <= server->m_WorkThreadCnt; i++)
            {
                printf(" Send TPS : %lld\n", arrTPS[i]);
            }
            printf(" ==================================\n");
        }
    }

    return 0;
}
unsigned ContentsThread(void *arg)
{
    size_t addr;
    CMessage *msg, *peekMessage;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    char *f, *r;
    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};
    DWORD hSignalIdx;
    ringBufferSize useSize, DeQSisze;
    ull l_sessionID;
    stHeader header;
    WORD type;
    // bool 이 좋아보임.
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"This is ContentsThread");
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"This is ContentsThread");
    }
    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
            break;

        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;

        useSize = server->m_ContentsQ.GetUseSize(f, r);

        // msg  크기 메세지 하나에 8Byte
        while (useSize >= 8)
        {

            DeQSisze = server->m_ContentsQ.Dequeue(&addr, sizeof(size_t));
            if (DeQSisze != sizeof(size_t))
                __debugbreak();

            msg = (CMessage *)addr;
            l_sessionID = msg->ownerID;

            *msg >> type;

            switch (type)
            {
            //현재 미 사용중
            case en_PACKET_Player_Alloc:
                server->AllocPlayer(msg);
                break;

            case en_PACKET_Player_Delete:
                server->DeletePlayer(msg);
                break;
            case en_PACKET_CS_CHAT_REQ_LOGIN:
                server->PacketProc(l_sessionID, msg, type);
                break;
            default:
                if (server->SessionID_hash.find(l_sessionID) == server->SessionID_hash.end())
                {
                    // Login Not Recv
                    stTlsObjectPool<CMessage>::Release(msg);
                    server->Disconnect(l_sessionID);
                }
                else
                    server->PacketProc(l_sessionID, msg,type);
            }

            f = server->m_ContentsQ._frontPtr;
            useSize -= 8;
        }
        DWORD currentTime;
        // 하트비트 부분
        {

            DWORD msgInterval;
            CPlayer *player;

            currentTime = timeGetTime();


            // prePlayer_hash 는 LoginPacket을 대기하는 Session
            for (auto element : server->prePlayer_hash)
            {
                player = element.second;

                msgInterval = currentTime - player->m_Timer;
          /*      if (msgInterval >= 50000)
                {
                    server->Disconnect(player->m_sessionID);
                }*/
            }
        }
        // 하트비트 부분
        {
   
            DWORD disTime;
            CPlayer *player;

            //AccountNo_hash 는 LoginPacket을 받아서 승격된 Player
            for (auto element : server->AccountNo_hash)
            {
                player = element.second;
  
                disTime = currentTime - player->m_Timer;
  /*              if (disTime >= 40000)
                {
                    server->Disconnect(player->m_sessionID);
                }*/
            }
        }
    }

    return 0;
}

void CTestServer::REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *ID, WCHAR *Nickname, WCHAR *SessionKey, BYTE byType, BYTE bBroadCast)
{
    CPlayer *player;

    // 옳바른 연결인지는 Token에 의존.
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    // 여기로 까지 왔다는 것은 로그인 서버의 인증을 통해 온  옳바른 연결이다.

    // Alloc을 받았다면 prePlayer_hash에 추가되어있을 것이다.
    if (prePlayer_hash.find(SessionID) == prePlayer_hash.end())
    {
        Proxy::RES_LOGIN(SessionID, msg, false, AccountNo);
        //없다는 것은 내가 만든 절차를 따르지않았음을 의미.
        //stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }
    player = prePlayer_hash[SessionID];
    prePlayer_hash.erase(SessionID);

    if (player->m_State != en_State::Session)
    {
        Proxy::RES_LOGIN(SessionID, msg, false, AccountNo);
        // 로그인 패킷을 여러번 보낸 경우.
        //stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        player_pool.Release(player);
        return;
    }

    // 중복 접속 문제
    if (SessionID_hash.find(AccountNo) != SessionID_hash.end())
    {
        Proxy::RES_LOGIN(SessionID, msg, false, AccountNo);

        // 중복 로그인 이면 둘 다 끊기
        //stTlsObjectPool<CMessage>::Release(msg);

        Disconnect(SessionID);
        // 이 경우때문에 결국 Player에 SessionID가 필요함.
        Disconnect(AccountNo_hash[AccountNo]->m_sessionID);

        return;
    }
    // 여기까지 와야 Player로 승격.
    //player = prePlayer_hash[SessionID];
    //prePlayer_hash.erase(SessionID);

    player->m_State = en_State::Player;
    player->m_AccountNo = AccountNo;

    memcpy(player->m_ID, ID,20);
    memcpy(player->m_Nickname, Nickname, 20);
    memcpy(player->m_SessionKey, SessionKey, 64);

    memcpy(player->m_SessionKey, SessionKey, sizeof(player->m_SessionKey));

    SessionID_hash[SessionID] = player;
    AccountNo_hash[AccountNo] = player;
    
    m_TotalPlayers++; // Player 카운트
    m_prePlayerCount--;

    {
        //Login응답.
        Proxy::RES_LOGIN(SessionID, msg, true, AccountNo );
    }
    
    SessionUnLock(SessionID);
}
void CTestServer::REQ_SECTOR_MOVE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD SectorX, WORD SectorY, BYTE byType , BYTE bBroadCast ) 
{
    CPlayer *player;

    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    
    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {
        __debugbreak();
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    player = SessionID_hash[SessionID];
    if (player->m_AccountNo != AccountNo)
    {

        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    g_Sector[player->iSectorY][player->iSectorX].erase(SessionID);

    player->iSectorX = SectorX;
    player->iSectorY = SectorY;
    g_Sector[player->iSectorY][player->iSectorX].insert(SessionID );

    Proxy::RES_SECTOR_MOVE(SessionID, msg, AccountNo, SectorX, SectorY);
    SessionUnLock(SessionID);
}
void CTestServer::REQ_MESSAGE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD MessageLen, WCHAR *MessageBuffer, BYTE byType , BYTE bBroadCast )
{
    CPlayer *player;
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {
        __debugbreak();
        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    player = SessionID_hash[SessionID];
    if (player->m_AccountNo != AccountNo)
    {

        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    Proxy::RES_MESSAGE(SessionID, msg, AccountNo, player->m_ID, player->m_Nickname, MessageLen, MessageBuffer);
    msg->iUseCnt = 0;

    int SectorX = player->iSectorX;
    int SectorY = player->iSectorY;

    st_Sector_Around AroundSectors;
    SectorManager::GetSectorAround(SectorX, SectorY, &AroundSectors);

    if (bBroadCast)
    {
        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            for (ull id : g_Sector[targetSector._iY][targetSector._iX])
            {
                _interlockedincrement64(&msg->iUseCnt);
   
            }
        }
        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            for (ull id : g_Sector[targetSector._iY][targetSector._iX])
            {
                UnitCast(id, msg);
            }
        }
    }
    SessionUnLock(SessionID);
}

void CTestServer::HEARTBEAT(ull SessionID, CMessage *msg, BYTE byType, BYTE bBroadCast)
{
    CPlayer *player;
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    player = SessionID_hash[SessionID];
    player->m_Timer = timeGetTime();

    Proxy::HEARTBEAT(SessionID, msg);

    SessionUnLock(SessionID);
}

void CTestServer::AllocPlayer(CMessage *msg)
{
    // AcceptThread 에서 삽입한 메세지.

    CPlayer *player;
    ull SessionID;

    InterlockedDecrement64(&m_AllocMsgCount);
    SessionID = msg->ownerID;
    // 옳바른 연결인지는 Token에 의존.
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID); // ?
        return;
    }
    //
    player = (CPlayer *)player_pool.Alloc();
    if (player == nullptr)
    {
        // Player가 가득 참.
        // TODO : 이 경우에는 끊기보다는 유예를 둬야하나?
        Disconnect(SessionID);
        return;
    }
    //이때 할당. 
    prePlayer_hash[SessionID] = player;
    // prePlayer_hash 는 Login을 기다리는 Session임.
    // LoginPacket을 받았다면 ;
    player->m_Timer = timeGetTime();
 
    player->m_sessionID = SessionID;
    player->m_State = en_State::Session;

    m_prePlayerCount++;
    SessionUnLock(SessionID);
    
}

void CTestServer::DeletePlayer(CMessage *msg)
{
    // TODO : 이 구현이 TestServer에 있는게 맞는가?
    //Player를 다루는 모든 자료구조에서 해당 Player를 제거.

    ull SessionID;
    CPlayer *player;
    SessionID = msg->ownerID;

    if (prePlayer_hash.find(SessionID) == prePlayer_hash.end())
    {
        // Player라면
        if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        {
            // 이 상황이 무슨 상황일까.
            __debugbreak();
        }
     
        player = SessionID_hash[SessionID];
        SessionID_hash.erase(SessionID);

        if (AccountNo_hash.find(player->m_AccountNo) == AccountNo_hash.end())
            __debugbreak();
        AccountNo_hash.erase(player->m_AccountNo);
        m_TotalPlayers--;
        
    }
    else
    {
        player = prePlayer_hash[SessionID];
        prePlayer_hash.erase(SessionID);
        m_prePlayerCount--;
    }
    stTlsObjectPool<CMessage>::Release(msg);
    player_pool.Release(player);


}


CTestServer::CTestServer(int iEncording)
    : CLanServer(iEncording)
{
    InitializeSRWLock(&srw_ContentQ);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);

    //TODO : 한계치를 정하는 함수 구현하기. 아직 미완임.
    player_pool.Initalize(m_maxPlayers);
    player_pool.Limite_Lock(); // Pool이 늘어나지않음. 

}

CTestServer::~CTestServer()
{
}

BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    BOOL retval;

    m_maxSessions = maxSessions;

    retval = CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);
    hContentsThread = (HANDLE)_beginthreadex(nullptr, 0, ContentsThread, this, 0, nullptr);
    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);

    return retval;
}

float CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    // double CurrentQ;
    ringBufferSize ContentsUseSize;

    stSRWLock srw(&srw_ContentQ);

    ContentsUseSize = m_ContentsQ.GetUseSize();
    if (msg == nullptr)
    {
        //ContentsQ 상태 확인용
        return float(ContentsUseSize) / float(CTestServer::s_ContentsQsize) * 100.f;
    }

    {
        Profiler profile;

        profile.Start(L"ContentsQ_Enqueue");

        // 포인터를 넣고
        CMessage **ppMsg;
        ppMsg = &msg;
        (*ppMsg)->ownerID = sessionID;

        if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
            __debugbreak();

        profile.End(L"ContentsQ_Enqueue");
        SetEvent(m_ContentsEvent);
    }

    return float(ContentsUseSize) / float(CTestServer::s_ContentsQsize) * 100.f;
}

bool CTestServer::OnAccept(ull SessionID)
{
    // Accept의 요청이 밀리고 있다고한다면, 그 이후에 Accept로 들어오는 Session을 끊겠다.
    // m_AllocMsgCount 처리량을 보여주는 변수.

    float qPersentage;
    LONG64 localAllocCnt;
    LONG64 local_IoCount;
    clsSession &session = sessions_vec[SessionID >> 47];

    localAllocCnt = _InterlockedIncrement64(&m_AllocMsgCount);

    // Alloc Message가 너무 쏟아진다면 인큐를 안함.
    if (localAllocCnt > m_AllocLimitCnt)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %4llu %10s %05lld  %10s %012llu  %10s %4llu %10s %05lld ",
                                       L"AllocMsg_Refuse",
                                       L"LocalMsgCnt", localAllocCnt,
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx);
        
        // 다시 감소 시키고, Session의 삭제 절차.
        localAllocCnt = _InterlockedDecrement64(&m_AllocMsgCount);
        // false를 반환하여 WSARecv를 걸지않고, Session에 대한 제거를 유도루틴을 타자.
        return false;
    }

    {
        CMessage *msg;
        
        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
        
        *msg << (unsigned short)en_PACKET_Player_Alloc;
        msg->ownerID = SessionID;
        if (_interlockedexchange64(&msg->iUseCnt, 1) != 0)
            __debugbreak();
        

        OnRecv(SessionID, msg);
    }

   

    return true;
}

void CTestServer::OnRelease(ull SessionID)
{
    {
        CMessage *msg;

        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

        *msg << (unsigned short)en_PACKET_Player_Delete;
        msg->ownerID = SessionID;
        if (_interlockedexchange64(&msg->iUseCnt, 1) != 0)
            __debugbreak();

        OnRecv(SessionID, msg);
    }

}

