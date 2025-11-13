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

            printf(" ==================================\n %-10s %lld \n", "Total Sessions :", server->GetSessionCount());
            printf(" ==================================\n %-10s %lld \n", "m_prePlayerCount :", server->m_prePlayerCount);
            printf(" ==================================\n %-10s %lld \n", "Total Players :", server->GetPlayerCount());

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
                __debugbreak();
                break;

            case en_PACKET_Player_Delete:
                server->DeletePlayer(msg);
                break;

            default:
                server->PacketProc(l_sessionID, msg, header,type);
            }

            f = server->m_ContentsQ._frontPtr;
            useSize -= 8;
        }
        // 하트비트 부분
        {
            DWORD currentTime;
            DWORD msgInterval;
            CPlayer *player;

            currentTime = timeGetTime();


            // prePlayer_hash 는 LoginPacket을 대기하는 Session
            for (auto element : server->prePlayer_hash)
            {
                player = element.second;

                msgInterval = currentTime - player->m_Timer;
                if (msgInterval >= 5000)
                {
                    server->Disconnect(player->m_sessionID);
                }
            }
        }
        // 하트비트 부분
        {
            DWORD currentTime;
            DWORD disTime;
            CPlayer *player;

            currentTime = timeGetTime();

            //AccountNo_hash 는 LoginPacket을 받아서 승격된 Player
            for (auto element : server->AccountNo_hash)
            {
                player = element.second;
  
                disTime = currentTime - player->m_Timer;
                if (disTime >= 40000)
                {
                    server->Disconnect(player->m_sessionID);
                }
            }
        }
    }

    return 0;
}

void CTestServer::EchoProcedure(ull SessionID, CMessage *msg, char *const buffer, short len)
{
    //해당 ID가 어느 섹터에 있는지, 보낼 대상을 특정하는 Logic
    Proxy::EchoProcedure(SessionID, msg, buffer,len);
}
void CTestServer::LoginProcedure(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey)
{
    //clsSession &session = sessions_vec[SessionID >> 47];
    CPlayer *player;

    // 옳바른 연결인지는 Token에 의존.
    // 

    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        
        return;
    }

    // 여기로 까지 왔다는 것은 로그인 서버의 인증을 통해 온  옳바른 연결이다.

    // Alloc을 받았다면 Hash에 추가되어있을 것이다.
    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {
        //없다는 것은 내가 만든 절차를 따르지않았음을 의미.
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        return;
    }

    player = SessionID_hash[SessionID];
    if (player->m_State != en_State::Session)
    {
        // 로그인 패킷을 여러번 보낸 경우.
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        SessionID_hash.erase(SessionID);
        AccountNo_hash.erase(SessionID);

        return;
    }

    // 이 경우때문에 결국 Player에 SessionID가 필요함.
    if (SessionID_hash.find(AccountNo) != SessionID_hash.end())
    {
        // 중복 로그인 이면 둘 다 끊기
        stTlsObjectPool<CMessage>::Release(msg);

        Disconnect(SessionID);
        Disconnect(AccountNo_hash[AccountNo]->m_sessionID);

        SessionID_hash.erase(AccountNo_hash[AccountNo]->m_sessionID);
        AccountNo_hash.erase(AccountNo_hash[AccountNo]->m_sessionID);

        SessionID_hash.erase(SessionID);
        AccountNo_hash.erase(SessionID);
        return;
    }
    // 여기까지 와야 Player로 승격.
    player->m_State = en_State::Player;
    m_TotalPlayers++; // Player 카운트
    m_prePlayerCount--;

    {
        //Login응답.
        Proxy::LoginProcedure(SessionID, msg, AccountNo);
    }
    
    SessionUnLock(SessionID);
}

void CTestServer::AllocPlayer(CMessage *msg)
{
    // AcceptThread 에서 삽입한 메세지.

    CPlayer *player;
    ull SessionID;
    ull AccountNo;

    *msg >> AccountNo;
    SessionID = msg->ownerID;

    SessionLock(SessionID);
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
    SessionID_hash[SessionID] = player;
    // AccountNo 는 Login을 통해야만 알 수있음.
    // AccountNo_hash[AccountNo] = player;

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

    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        __debugbreak();
    player = SessionID_hash[SessionID];
    SessionID_hash.erase(SessionID);

    if (AccountNo_hash.find(player->m_AccountNo) == AccountNo_hash.end())
        __debugbreak();
    SessionID_hash.erase(player->m_AccountNo);

    player_pool.Release(player);
    m_TotalPlayers--;

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
    // ContentsQ 에 PlayerAlloc 삽입 안하는 방향으로 가보는 중
    {
        CMessage *msg;
        
        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
        
        *msg << (unsigned short)en_PACKET_Player_Alloc;
        *msg << SessionID;

        {
            stSRWLock lock(&srw_ContentQ);

            CMessage **ppMsg;
            ppMsg = &msg;
            (*ppMsg)->ownerID = SessionID;

            if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
                __debugbreak();

            SetEvent(m_ContentsEvent);
        }
       
    }

   

    return true;
}

