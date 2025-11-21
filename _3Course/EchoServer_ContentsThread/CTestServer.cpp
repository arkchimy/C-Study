#include "stdafx.h"
#include "CTestServer.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include <thread>
#include "../lib/CNetwork_lib/Proxy.h"

#pragma comment(lib, "Winmm.lib")

extern SRWLOCK srw_Log;
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

void MsgTypePrint(WORD type, LONG64 val);

unsigned MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    HANDLE hWaitHandle = {server->m_ServerOffEvent};

    DWORD wait_retval;

    LONG64 UpdateTPS ;
    LONG64 before_UpdateTPS = 0;

    LONG64 RecvTPS ;
    LONG64 before_RecvTPS = 0;

    LONG64 *arrTPS;
    LONG64 *before_arrTPS;

    arrTPS = new LONG64[server->m_WorkThreadCnt + 1]; //Accept + 1
    before_arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));
    ZeroMemory(before_arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));

    LONG64 RecvMsgArr[en_PACKET_CS_CHAT__Max]{0,};
    LONG64 before_RecvMsgArr[en_PACKET_CS_CHAT__Max]{0,};


    // 얘는 signal 말고 전역변수로 끄자.

    timeBeginPeriod(1);

    DWORD currentTime = timeGetTime();
    DWORD nextTime; // 내가 목표로하는 이상적인 시간.
    nextTime = currentTime;

    ////ServerInfo

        int workthreadCnt = server->m_WorkThreadCnt;
        bool ZeroCopy = server->bZeroCopy;
        int MaxSessions = server->sessions_vec.size();
        int maxPlayers = server->m_maxPlayers;
        int bNoDelay = server->bNoDelay;

    while (1)
    {
        nextTime += 1000;
        {
            LONG64 old_UpdateTPS = server->m_UpdateTPS;
            UpdateTPS = old_UpdateTPS - before_UpdateTPS;
            before_UpdateTPS = old_UpdateTPS;
        }
        for (int i = 0; i <= workthreadCnt; i++)
        {
            LONG64 old_arrTPS = server->arrTPS[i];
            arrTPS[i] = old_arrTPS - before_arrTPS[i];
            before_arrTPS[i] = old_arrTPS;
        }
        printf(" ==================================\n ");
        printf("%20s %10d \n", "WorkerThread Cnt :", workthreadCnt);
        printf("%20s %10d \n", "ZeroCopy  :", ZeroCopy);
        printf("%20s %10d \n", "Nodelay  :", bNoDelay);
        printf("%20s %10d \n", "ContentsQSize  :", server->m_ContentsQ._size);
        printf("%20s %10d \n", "MaxSessions  :", MaxSessions);
        printf("%20s %10d \n", "maxPlayers  :", maxPlayers);

        printf(" ==================================\n ");
        printf("%20s %10lld \n", "SessionNum :", server->GetSessionCount());
        printf("%20s %10lld \n", "PacketPool :", stTlsObjectPool<CMessage>::instance.m_TotalCount);

        printf("%20s %10lld \n", "UpdateMessage_Queue :", server->m_UpdateMessage_Queue);
        printf("%20s %10lld \n", "UpdateMessage_Pool :", server->getNetworkMsgCount());
    

        printf("%20s %10lld \n", "prePlayer Count:", server->GetprePlayer_hash());
        printf("%20s %10lld \n", "Player Count:", server->GetPlayerCount());


        printf("%20s %10lld \n", "AccountNo_hash_size:", server->GetAccountNo_hash());
        printf("%20s %10lld \n", "SessionID_hash_size:", server->GetSessionID_hash());



        printf (" ==================================\n ");

        printf(" Total iDisCounnectCount: %llu\n", server->iDisCounnectCount);

        printf(" TotalAccept : %llu\n", server->getTotalAccept());
        printf(" Accept TPS : %lld\n", arrTPS[0]);

        printf(" Update  TPS : %lld\n", UpdateTPS);
        //Update TPS : - Update 처리 초당 횟수
        //Recv TPS : - 초당 Recv 메시지 수
        //Send TPS : -초당 Send 메시지 수
        //메시지별 수신 TPS

        {
            LONG64 old_RecvTPS = server->m_RecvTPS;
            RecvTPS = old_RecvTPS - before_RecvTPS;
            before_RecvTPS = old_RecvTPS;
            printf(" Recv TPS : %lld\n", RecvTPS);
        }
        {
            LONG64 sum = 0;
            for (int i = 1; i <= server->m_WorkThreadCnt; i++)
            {
                sum += arrTPS[i];
                //printf(" Send TPS : %lld\n", arrTPS[i]);
            }
            printf(" Send TPS : %lld\n", sum);
        }

        printf(" ==================================\n");

        // 메세지 별
        for (int i = 3; i < en_PACKET_CS_CHAT__Max - 1; i++)
        {
            LONG64 old_RecvMsg = server->m_RecvMsgArr[i];
            RecvMsgArr[i] = old_RecvMsg - before_RecvMsgArr[i];
            before_RecvMsgArr[i] = old_RecvMsg;
            MsgTypePrint(i, RecvMsgArr[i]);
        }

        currentTime = timeGetTime();
        if (nextTime > currentTime)
            Sleep(nextTime - currentTime);
    }

    return 0;
}
unsigned ContentsThread(void *arg)
{
    DWORD hSignalIdx;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};

    // bool 이 좋아보임.
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"This is ContentsThread");
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"This is ContentsThread");
    }
    ringBufferSize ContentsUseSize;
    char *f, *r;

    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
            break;
      
        server->Update();
        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;
        
        ContentsUseSize = server->m_ContentsQ.GetUseSize(f,r);
        server->m_UpdateMessage_Queue = (LONG64)(ContentsUseSize / 8);

        server->prePlayer_hash_size = server->prePlayer_hash.size();
        server->AccountNo_hash_size = server->AccountNo_hash.size();
        server->SessionID_hash_size = server->SessionID_hash.size();
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

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                       L"%-20s %05lld %12s %05llu %12s %05llu ",
                                       L"LoginError - prePlayer_hash not found : ", AccountNo,
                                       L"현재들어온ID:", SessionID);
        Disconnect(SessionID);
        SessionUnLock(SessionID);
        return;
    }
    player = prePlayer_hash[SessionID];

    if (player->m_State != en_State::Session)
    {

        Proxy::RES_LOGIN(SessionID, msg, false, AccountNo);
        // 로그인 패킷을 여러번 보낸 경우.

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %05lld %12s %05lld %12s %05llu ",
                                       L"LoginError - state not equle Session : ", AccountNo,
                                       L"현재들어온ID:", SessionID
                                       );

        Disconnect(SessionID);
        SessionUnLock(SessionID);
        return;
    }

    // 중복 접속 문제
    if (AccountNo_hash.find(AccountNo) != AccountNo_hash.end())
    {
        Proxy::RES_LOGIN(SessionID, msg, false, AccountNo);

        // 중복 로그인 이면 둘 다 끊기

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                L"%-20s %05lld %12s %05lld %12s %05llu ",
                                       L"중복접속문제Account : ", AccountNo,
                                       L"현재들어온ID:", SessionID,
                                       L"현재들어온ID:", AccountNo_hash[AccountNo]->m_sessionID);
        Disconnect(SessionID);
        // 이 경우때문에 결국 Player에 SessionID가 필요함.
        Disconnect(AccountNo_hash[AccountNo]->m_sessionID);
        SessionUnLock(SessionID);
        return;
    }
    // 여기까지 와야 Player로 승격.
    //player = prePlayer_hash[SessionID];


    player->m_State = en_State::Player;
    player->m_AccountNo = AccountNo;

    memcpy(player->m_ID, ID,20);
    memcpy(player->m_Nickname, Nickname, 20);
    memcpy(player->m_SessionKey, SessionKey, 64);

    memcpy(player->m_SessionKey, SessionKey, sizeof(player->m_SessionKey));

    SessionID_hash[SessionID] = player;
    AccountNo_hash[AccountNo] = player;

    prePlayer_hash.erase(SessionID);

    player->m_Timer = timeGetTime();

    m_TotalPlayers++; // Player 카운트
    m_prePlayerCount--;

    {
        //Login응답.
        Proxy::RES_LOGIN(SessionID, msg, true, AccountNo );
        m_RecvMsgArr[en_PACKET_CS_CHAT_RES_LOGIN]++;
    }
    CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                   L"%-20s %05lld %12s %05llu  ",
                                   L"Login - Accept : ", AccountNo,
                                   L"현재들어온ID:", SessionID);
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
        SessionUnLock(SessionID);
        return;
    }
    player = SessionID_hash[SessionID];
    if (player->m_AccountNo != AccountNo)
    {

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                L"%-20s %12s %05llu %12s %05lld ",
                                L"REQ_SECTOR_MOVE m_AccountNo != AccountNo : ",
                                L"현재들어온ID:", SessionID,
                                L"현재들어온Account:", AccountNo,
                                L"player Account:", player->m_AccountNo);

        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        SessionUnLock(SessionID);
        return;
    }

    g_Sector[player->iSectorY][player->iSectorX].erase(SessionID);

    player->iSectorX = SectorX;
    player->iSectorY = SectorY;
    g_Sector[player->iSectorY][player->iSectorX].insert(SessionID );

    Proxy::RES_SECTOR_MOVE(SessionID, msg, AccountNo, SectorX, SectorY);
    m_RecvMsgArr[en_PACKET_CS_CHAT_RES_SECTOR_MOVE]++;
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
        SessionUnLock(SessionID);
        return;
    }
    player = SessionID_hash[SessionID];
    if (player->m_AccountNo != AccountNo)
    {

        
        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %12s %05llu %12s %05lld ",
                                       L"REQ_MESSAGE m_AccountNo != AccountNo : ",
                                       L"현재들어온ID:", SessionID,
                                       L"현재들어온Account:", AccountNo,
                                       L"player Account:", player->m_AccountNo);

        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        SessionUnLock(SessionID);
        return;
    }
    //TODO : Broad Cast  방식 수정하기.
    Proxy::RES_MESSAGE(SessionID, msg, AccountNo, player->m_ID, player->m_Nickname, MessageLen, MessageBuffer);

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
                if (id == SessionID)
                    continue;

                msg->iUseCnt++;
   
            }
        }
        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            for (ull id : g_Sector[targetSector._iY][targetSector._iX])
            {
                //여기가 문제인것 같은데. 
             /*   if (SessionLock(id) == false)
                {
                    stTlsObjectPool<CMessage>::Release(msg);
                    continue;
                }*/
                if (SessionID_hash.find(id) != SessionID_hash.end())
                {
                    player = SessionID_hash[id];
                    UnitCast(id, msg, player->m_AccountNo);
                    m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]++;
                }
                //SessionUnLock(id);
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
    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {
 
        stTlsObjectPool<CMessage>::Release(msg);

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %12s %05llu %12s %05llu ",
                                       L"HEARTBEAT SessionID_hash not Found : ",
                                       L"현재들어온ID:", SessionID
                                   );

        Disconnect(SessionID);
        SessionUnLock(SessionID);
        return;
    }
    player = SessionID_hash[SessionID];
    player->m_Timer = timeGetTime();

    CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                   L"%-20s %12s %05llu %12s %05llu ",
                                   L"HEARTBEAT Send : ",
                                   L"현재들어온ID:", SessionID);

    stTlsObjectPool<CMessage>::Release(msg);
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

        //CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
        //                               L"%-20s %12s %05llu %12s %05llu ",
        //                               L"AllocPlayer SessionLock Failed : ",
        //                               L"현재들어온ID:", SessionID);

        Disconnect(SessionID); // ?
        return;
    }
    //
    player = (CPlayer *)player_pool.Alloc();
    if (player == nullptr)
    {
        // Player가 가득 참.
        // TODO : 이 경우에는 끊기보다는 유예를 둬야하나?
 
        stTlsObjectPool<CMessage>::Release(msg);
        Disconnect(SessionID);
        SessionUnLock(SessionID);
        return;
    }
    //이때 할당. 
    prePlayer_hash[SessionID] = player;
    // prePlayer_hash 는 Login을 기다리는 Session임.
    // LoginPacket을 받았다면 ;
    // 
    CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                L"%-20s %12s %05llu %12s %05llu ",
                                L"AllocPlayer SessionLock Failed : ",
                                L"현재들어온ID:", SessionID);

    player->m_Timer = timeGetTime();
 
    player->m_sessionID = SessionID;
    player->m_State = en_State::Session;

    m_prePlayerCount++;
    SessionUnLock(SessionID);
    stTlsObjectPool<CMessage>::Release(msg);
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
            //Alloc을 처리하기도 전에. 연결이 끊어진 경우
        }
        else
        {
            player = SessionID_hash[SessionID];
            SessionID_hash.erase(SessionID);

            if (AccountNo_hash.find(player->m_AccountNo) == AccountNo_hash.end())
                __debugbreak();
            AccountNo_hash.erase(player->m_AccountNo);
            m_TotalPlayers--;

            g_Sector[player->iSectorY][player->iSectorX].erase(SessionID);

            player->m_State = en_State::DisConnect;

            CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                           L"%-20s %05lld %12s %05llu  ",
                                           L"LogOut - Accept : ", player->m_AccountNo,
                                           L"현재들어온ID:", SessionID);

            player_pool.Release(player);
        }
        
    }
    else
    {
        // Accept 이후  DeletePlayer가 LoginPacket보다 먼저 옴.
        player = prePlayer_hash[SessionID];
        prePlayer_hash.erase(SessionID);
        m_prePlayerCount--;

        player->m_State = en_State::DisConnect;
        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                       L"%-20s %05lld %12s %05llu  ",
                                       L"LoginBeforeOut - WastAccept : ", player->m_AccountNo,
                                       L"현재들어온ID:", SessionID);

        player_pool.Release(player);


    }

    stTlsObjectPool<CMessage>::Release(msg);
}


CTestServer::CTestServer(int iEncording)
    : CLanServer(iEncording)
{
    InitializeSRWLock(&srw_ContentQ);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);

    //TODO : 한계치를 정하는 함수 구현하기
    player_pool.Initalize(m_maxPlayers);
    player_pool.Limite_Lock(); // Pool이 늘어나지않음. 

}

CTestServer::~CTestServer()
{
}

void CTestServer::Update()
{

    size_t addr;
    CMessage *msg;

    char *f, *r;

    ringBufferSize useSize, DeQSisze;
    ull l_sessionID;

    WORD type;


    // msg  크기 메세지 하나에 8Byte
    while (m_ContentsQ.GetUseSize() != 0)
    {

        DeQSisze = m_ContentsQ.Dequeue(&addr, sizeof(size_t));
        if (DeQSisze != sizeof(size_t))
            __debugbreak();

        msg = (CMessage *)addr;
        l_sessionID = msg->ownerID;

        *msg >> type;

        switch (type)
        {
        // 현재 미 사용중
        case en_PACKET_Player_Alloc:
            AllocPlayer(msg);
            _InterlockedDecrement64(&m_NetworkMsgCount);
            break;

        case en_PACKET_Player_Delete:
            DeletePlayer(msg);
            _InterlockedDecrement64(&m_NetworkMsgCount);
            break;
        case en_PACKET_CS_CHAT_REQ_LOGIN:
            PacketProc(l_sessionID, msg, type);
            break;
        default:
            if (SessionID_hash.find(l_sessionID) == SessionID_hash.end())
            {
                // Login Not Recv
                CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %12s %05llu %12s %05llu ",
                                               L"HEARTBEAT SessionID_hash not Found : ",
                                               L"현재들어온ID:", l_sessionID);

                stTlsObjectPool<CMessage>::Release(msg);
                Disconnect(l_sessionID);
            }
            else
            {   // Client Message
                CPlayer *player = SessionID_hash[l_sessionID];

                player->m_Timer = timeGetTime();
                PacketProc(l_sessionID, msg, type);
                m_RecvMsgArr[type]++;
            }
        }
      
        m_UpdateTPS++;
    }

    DWORD currentTime;
    currentTime = timeGetTime();
    //// LoginPacket을 대기하는 하트비트 부분
    {

        DWORD msgInterval;
        CPlayer *player;

        // prePlayer_hash 는 LoginPacket을 대기하는 Session
        for (auto& element : prePlayer_hash)
        {
            player = element.second;

            msgInterval = currentTime - player->m_Timer;
            if (msgInterval >= 3000)
            {
                Disconnect(player->m_sessionID);
            }
        }
    }
    //// LoginPacket을 받아서 승격된 하트비트 부분
    {

        DWORD disTime;
        CPlayer *player;

        // AccountNo_hash 는 LoginPacket을 받아서 승격된 Player
        for (auto& element : AccountNo_hash)
        {
            player = element.second;

            disTime = currentTime - player->m_Timer;
            if (disTime >= 40000)
            {
                Disconnect(player->m_sessionID);
            }
        }
    }
}

BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    BOOL retval;
    HRESULT hr;

    m_maxSessions = maxSessions;

    retval = CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);
    hContentsThread = (HANDLE)_beginthreadex(nullptr, 0, ContentsThread, this, 0, nullptr);
    RT_ASSERT(hContentsThread != nullptr);
    hr = SetThreadDescription(hContentsThread, L"\tContentsThread");
    RT_ASSERT(!FAILED(hr));

    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);
    RT_ASSERT(hMonitorThread != nullptr);
    hr = SetThreadDescription(hMonitorThread, L"\tMonitorThread");
    RT_ASSERT(!FAILED(hr));

    return retval;
}

float CTestServer::OnRecv(ull SessionID, CMessage *msg)
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
        InterlockedExchange(&msg->ownerID, SessionID);
      

        if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
            __debugbreak();

        profile.End(L"ContentsQ_Enqueue");
        SetEvent(m_ContentsEvent);
        //
        _interlockedincrement64(&m_RecvTPS);
    }

    return float(ContentsUseSize) / float(CTestServer::s_ContentsQsize) * 100.f;
}

bool CTestServer::OnAccept(ull SessionID)
{
    // Accept의 요청이 밀리고 있다고한다면, 그 이후에 Accept로 들어오는 Session을 끊겠다.
    // m_AllocMsgCount 처리량을 보여주는 변수.

    LONG64 localAllocCnt;

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
        InterlockedExchange(&msg->ownerID, SessionID);
        //TODO : 실패의 경우의 수
        OnRecv(SessionID, msg);
        _InterlockedIncrement64(&m_NetworkMsgCount);
    }

   

    return true;
}

void CTestServer::OnRelease(ull SessionID)
{
    {
        CMessage *msg;

        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

        *msg << (unsigned short)en_PACKET_Player_Delete;
        InterlockedExchange(&msg->ownerID, SessionID);

        _InterlockedIncrement64(&m_NetworkMsgCount);
        OnRecv(SessionID, msg);
    }

}

void MsgTypePrint(WORD type, LONG64 val)
{
    //0 은 ㄴㄴ
    static const char *Msgformat[en_PACKET_CS_CHAT__Max] = {
        "",
        "en_PACKET_CS_CHAT_REQ_LOGIN",
        "en_PACKET_CS_CHAT_RES_LOGIN",
        "en_PACKET_CS_CHAT_REQ_SECTOR_MOVE",
        "en_PACKET_CS_CHAT_RES_SECTOR_MOVE",
        "en_PACKET_CS_CHAT_REQ_MESSAGE",
        "en_PACKET_CS_CHAT_RES_MESSAGE",
        "en_PACKET_CS_CHAT__HEARTBEAT",

    };
    printf("%20s : %05llu\n", Msgformat[type], val);

}
