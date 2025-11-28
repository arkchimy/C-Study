
#include "stdafx.h"
#include "CMTChattingServer.h"

#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include "../lib/CNetwork_lib/Proxy.h"
#include "../lib/CNetwork_lib/Stub.h"

#include <thread>

#include <algorithm>

#pragma comment(lib, "Winmm.lib")

extern SRWLOCK srw_Log;
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

void MsgTypePrint(WORD type, LONG64 val);

unsigned MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    HANDLE hWaitHandle = {server->m_ServerOffEvent};

    LONG64 UpdateTPS;
    LONG64 before_UpdateTPS = 0;

    LONG64 RecvTPS;
    LONG64 before_RecvTPS = 0;

    LONG64 *arrTPS;
    LONG64 *before_arrTPS;

    arrTPS = new LONG64[server->m_WorkThreadCnt + 1]; // Accept + 1
    before_arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));
    ZeroMemory(before_arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));

    LONG64 RecvMsgArr[en_PACKET_CS_CHAT__Max]{
        0,
    };
    LONG64 before_RecvMsgArr[en_PACKET_CS_CHAT__Max]{
        0,
    };

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
        printf("%20s %10d \n", "ContentsQSize  :", server->s_ContentsQsize);
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

        printf(" ==================================\n ");

        printf(" Total iDisCounnectCount: %llu\n", server->iDisCounnectCount);

        printf(" TotalAccept : %llu\n", server->getTotalAccept());
        printf(" Accept TPS : %lld\n", arrTPS[0]);

        printf(" Update  TPS : %lld\n", UpdateTPS);
        // Update TPS : - Update 처리 초당 횟수
        // Recv TPS : - 초당 Recv 메시지 수
        // Send TPS : -초당 Send 메시지 수
        // 메시지별 수신 TPS

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
                // printf(" Send TPS : %lld\n", arrTPS[i]);
            }
            printf(" Send TPS : %lld\n", sum);
        }
        printf(" ==================================\n");
        {
            LONG64 sum = 0;
            for (auto element : server->balanceVec)
            {
                printf(" Contetent Sessiond : %d \n", element.second);
            }
            
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
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"MonitorThread Terminated %d", 0);
    return 0;
}
//arg[0] pServer
//arg[1] pContentsQ

void BalanceThread(void *arg)
{
    DWORD hSignalIdx;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    CRingBuffer *CotentsQ;
    HANDLE local_ContentsEvent;


    ringBufferSize ContentsUseSize;
    char *f, *r;

    {
        CotentsQ = &server->m_BalanceQ;
        local_ContentsEvent = server->hBalanceEvent;
    }

    HANDLE hWaitHandle[2] = {local_ContentsEvent, server->m_ServerOffEvent};

    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"BalanceThread");
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"BalanceThread");
    }


    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);


        server->BalanceUpdate();

        f = CotentsQ->_frontPtr;
        r = CotentsQ->_rearPtr;

        ContentsUseSize = CotentsQ->GetUseSize(f, r);
        server->m_UpdateMessage_Queue = (LONG64)(ContentsUseSize / 8);

        server->prePlayer_hash_size = server->prePlayer_hash.size();
        server->AccountNo_hash_size = server->AccountNo_hash.size();
        server->SessionID_hash_size = server->SessionID_hash.size();

        if (hSignalIdx - WAIT_OBJECT_0 == 1 && server->prePlayer_hash_size == 0 && server->AccountNo_hash_size == 0 && server->SessionID_hash_size == 0 && ContentsUseSize == 0)
        {
            CloseHandle(server->m_hIOCP);
            break;
        }
        else if (hSignalIdx - WAIT_OBJECT_0 == 1)
        {
            CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode,
                                           L"BalanceThread wait Player Exit %lld  %lld %lld",
                                           server->prePlayer_hash_size,
                                           server->AccountNo_hash_size,
                                           server->SessionID_hash_size);
        }
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"BalanceThread Terminated %d", 0);
    //return 0;
}


thread_local ull tls_ContentsQIdx;  // ContentsThread 에서 사용하는 Q에  접근하기위한 Idx
void ContentsThread(void *arg)
{
    DWORD hSignalIdx;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    CRingBuffer *CotentsQ;
    HANDLE local_ContentsEvent;



    {
        tls_ContentsQIdx = InterlockedIncrement(&server->m_ContentsThreadIdX);
        CotentsQ = &server->m_CotentsQ_vec[tls_ContentsQIdx];
        local_ContentsEvent = server->m_ContentsQMap[CotentsQ].second;
    }

    HANDLE hWaitHandle[2] = {local_ContentsEvent, server->m_ServerOffEvent};

    // bool 이 좋아보임.
    {
        std::wstring ContentsThreadName;
        WCHAR* DS;
        GetThreadDescription(GetCurrentThread(), &DS);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       DS);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       DS);
    }
    ringBufferSize ContentsUseSize;
    char *f, *r;

    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
        {
            server->Update();
            break;
        }
        server->Update();

        f = CotentsQ->_frontPtr;
        r = CotentsQ->_rearPtr;

        ContentsUseSize = CotentsQ->GetUseSize(f, r);
        server->m_UpdateMessage_Queue = (LONG64)(ContentsUseSize / 8);

        server->prePlayer_hash_size = server->prePlayer_hash.size();
        server->AccountNo_hash_size = server->AccountNo_hash.size();
        server->SessionID_hash_size = server->SessionID_hash.size();
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"ContentsThread Terminated %d", 0);

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
        // 없다는 것은 내가 만든 절차를 따르지않았음을 의미.

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
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
                                       L"현재들어온ID:", SessionID);

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
    // player = prePlayer_hash[SessionID];

    std::vector<std::pair<DWORD, int>>::iterator iter = std::min_element(balanceVec.begin(), balanceVec.end(),
                                  [](const std::pair<DWORD, int> &a, const std::pair<DWORD, int> &b)
                                {
                                    return a.second < b.second; // value 기준으로 비교
                                });
    DWORD idx = iter->first;
    iter->second++;

    player->m_State = en_State::Player;
    player->m_AccountNo = AccountNo;

    memcpy(player->m_ID, ID, 20);
    memcpy(player->m_Nickname, Nickname, 20);
    memcpy(player->m_SessionKey, SessionKey, 64);

    memcpy(player->m_SessionKey, SessionKey, sizeof(player->m_SessionKey));

    SessionID_hash[SessionID] = player;
    AccountNo_hash[AccountNo] = player;

    prePlayer_hash.erase(SessionID);

    InterlockedExchange(&player->m_Timer, timeGetTime());

    m_TotalPlayers++; // Player 카운트
    m_prePlayerCount--;


    _InterlockedExchange(&player->m_ContentsQIdx, idx);

    {
        // Login응답.
        Proxy::RES_LOGIN(SessionID, msg, true, AccountNo);

        InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_LOGIN]);
    }
    CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                   L"%-20s %05lld %12s %05llu  ",
                                   L"Login - Accept : ", AccountNo,
                                   L"현재들어온ID:", SessionID);



    SessionUnLock(SessionID);
}
void CTestServer::REQ_SECTOR_MOVE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD SectorX, WORD SectorY, BYTE byType, BYTE bBroadCast)
{
    CPlayer *player;
    WORD beforeX, beforeY;

    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    AcquireSRWLockShared(&srw_SessionID_Hash);

    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {
        __debugbreak();
        stTlsObjectPool<CMessage>::Release(msg);
        SessionUnLock(SessionID);

        ReleaseSRWLockShared(&srw_SessionID_Hash);
        return;
    }
    player = SessionID_hash[SessionID];

    beforeX = player->iSectorX;
    beforeY = player->iSectorY;

    player->iSectorX = SectorX;
    player->iSectorY = SectorY;

    ReleaseSRWLockShared(&srw_SessionID_Hash);

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

    {
        stSRWLock srw(&srw_Sectors[beforeY][beforeX]);
        g_Sector[beforeY][beforeX].erase(SessionID);
    }



    {
        stSRWLock srw(&srw_Sectors[SectorY][SectorX]);
        g_Sector[SectorY][SectorX].insert(SessionID);
    }

    Proxy::RES_SECTOR_MOVE(SessionID, msg, AccountNo, SectorX, SectorY);

    InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_SECTOR_MOVE]);

    SessionUnLock(SessionID);
}
void CTestServer::REQ_MESSAGE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD MessageLen, WCHAR *MessageBuffer, BYTE byType, BYTE bBroadCast)
{
    CPlayer *player;
    int SectorX;
    int SectorY;
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    AcquireSRWLockShared(&srw_SessionID_Hash);
    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {
        __debugbreak();
        Disconnect(SessionID);
        stTlsObjectPool<CMessage>::Release(msg);
        SessionUnLock(SessionID);
        ReleaseSRWLockShared(&srw_SessionID_Hash);
        return;
    }

    player = SessionID_hash[SessionID];

    SectorX = player->iSectorX;
    SectorY = player->iSectorY;

    

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
        ReleaseSRWLockShared(&srw_SessionID_Hash);
        return;
    }
    // TODO : Broad Cast  방식 수정하기.
    Proxy::RES_MESSAGE(SessionID, msg, AccountNo, player->m_ID, player->m_Nickname, MessageLen, MessageBuffer);

    st_Sector_Around AroundSectors;
    SectorManager::GetSectorAround(SectorX, SectorY, &AroundSectors);

    LONG64 SendCnt = 0;
    std::list<ull> sendID;

    if (bBroadCast)
    {

        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            AcquireSRWLockShared(&srw_Sectors[targetSector._iY][targetSector._iX]);
        }

        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            for (ull id : g_Sector[targetSector._iY][targetSector._iX])
            {
                if (SessionID_hash.find(id) != SessionID_hash.end())
                {
                    sendID.push_back(id);
                    SendCnt++;
                }
            }
        }
        InterlockedExchange64(&msg->iUseCnt, SendCnt);

        for (ull id : sendID)
        {
            player = SessionID_hash[id];
            UnitCast(id, msg, player->m_AccountNo);
            SendCnt--;
            InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]);
        }
        //TODO : 이유 찾기
  
        //for (const st_Sector_Pos targetSector : AroundSectors.Around)
        //{
        //    for (ull id : g_Sector[targetSector._iY][targetSector._iX])
        //    {
        //        
        //        if (SessionID_hash.find(id) != SessionID_hash.end())
        //        {
        //            player = SessionID_hash[id];
        //            UnitCast(id, msg, player->m_AccountNo);
        //            SendCnt--;
        //            InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]);
        //        }
        //        
        //        // SessionUnLock(id);
        //    }
        //}
        if (SendCnt != 0)
            __debugbreak();

        ReleaseSRWLockShared(&srw_SessionID_Hash);

        for (const st_Sector_Pos targetSector : AroundSectors.Around)
        {
            ReleaseSRWLockShared(&srw_Sectors[targetSector._iY][targetSector._iX]);
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
    AcquireSRWLockShared(&srw_SessionID_Hash);
    if (SessionID_hash.find(SessionID) == SessionID_hash.end())
    {

        stTlsObjectPool<CMessage>::Release(msg);

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                        L"%-20s %12s %05llu %12s %05llu ",
                                        L"HEARTBEAT SessionID_hash not Found : ",
                                        L"현재들어온ID:", SessionID);

        Disconnect(SessionID);
        SessionUnLock(SessionID);
        ReleaseSRWLockShared(&srw_SessionID_Hash);
        return;
    }

    player = SessionID_hash[SessionID];
    InterlockedExchange(&player->m_Timer, timeGetTime());

    ReleaseSRWLockShared(&srw_SessionID_Hash);



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
        return;
    }
    //
    player = (CPlayer *)player_pool.Alloc();
    if (player == nullptr)
    {
        // Player가 가득 참.
        // TODO : 이 경우에는 끊기보다는 유예를 둬야하나?

        stTlsObjectPool<CMessage>::Release(msg);

        CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %05lld %12s %05llu %12s %05llu ",
                                       L"Player is Fulled : ", 0,
                                       L"현재들어온ID:", SessionID);

        Disconnect(SessionID);
        SessionUnLock(SessionID);
        return;
    }
    // 이때 할당.
    prePlayer_hash[SessionID] = player;
    // prePlayer_hash 는 Login을 기다리는 Session임.
    // LoginPacket을 받았다면 ;
    //
    CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                   L"%-20s %12s %05llu %12s %05llu ",
                                   L"AllocPlayer SessionLock Failed : ",
                                   L"현재들어온ID:", SessionID);
    InterlockedExchange(&player->m_Timer, timeGetTime());

    player->m_sessionID = SessionID;
    player->m_State = en_State::Session;


    m_prePlayerCount++;
    SessionUnLock(SessionID);
    stTlsObjectPool<CMessage>::Release(msg);
}

void CTestServer::DeletePlayer(CMessage *msg)
{
    // TODO : 이 구현이 TestServer에 있는게 맞는가?
    // Player를 다루는 모든 자료구조에서 해당 Player를 제거.

    ull SessionID;
    CPlayer *player;
    SessionID = msg->ownerID;

    if (prePlayer_hash.find(SessionID) == prePlayer_hash.end())
    {
        // Player라면
  
        if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        {
            // 이 상황이 무슨 상황일까.
            // Alloc을 처리하기도 전에. 연결이 끊어진 경우
        }
        else
        {
            player = SessionID_hash[SessionID];
            SessionID_hash.erase(SessionID);

            if (AccountNo_hash.find(player->m_AccountNo) == AccountNo_hash.end())
            {

                __debugbreak();
            }

            AccountNo_hash.erase(player->m_AccountNo);  

            m_TotalPlayers--;

            AcquireSRWLockExclusive(&srw_Sectors[player->iSectorY][player->iSectorX]);
            g_Sector[player->iSectorY][player->iSectorX].erase(SessionID);
            ReleaseSRWLockExclusive(&srw_Sectors[player->iSectorY][player->iSectorX]);

            player->m_State = en_State::DisConnect;

            CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::DEBUG_TargetMode,
                                           L"%-20s %05lld %12s %05llu  ",
                                           L"LogOut - Accept : ", player->m_AccountNo,
                                           L"현재들어온ID:", SessionID);

            for (auto& element : balanceVec)
            {
                if (element.first == player->m_ContentsQIdx)
                {
                    element.second--;
                    break;
                }
            }
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

CTestServer::CTestServer(DWORD ContentsThreadCnt, int iEncording)
    : CLanServer(iEncording), m_ContentsThreadCnt(ContentsThreadCnt), m_RecvTPS(0), m_UpdateTPS(0), m_UpdateMessage_Queue(0), hBalanceThread(0)
{
    HRESULT hr;
    pBalanceThread = std::thread (&BalanceThread, this);
    hr = SetThreadDescription(pBalanceThread.native_handle(), L"\tBalanceThread");
    RT_ASSERT(!FAILED(hr));

    InitializeSRWLock(&srw_SessionID_Hash);// SessionID_hash 소유권.

    // BalanceThread를 위한 정보 초기화.
    {
        InitializeSRWLock(&srw_BalanceQ);
        hBalanceEvent = CreateEvent(nullptr, false, false, nullptr);
    }
    // ContentsThread의 생성
    hContentsThread_vec.resize(m_ContentsThreadCnt);

    for (DWORD i = 0; i < m_ContentsThreadCnt; i++)
    {
        std::wstring ContentsThreadName = L"\tContentsThread" + std::to_wstring(i);

        hContentsThread_vec[i] = std::move(std::thread(ContentsThread, this));
        RT_ASSERT(hContentsThread_vec[i].native_handle() != nullptr);

        hr = SetThreadDescription(hContentsThread_vec[i].native_handle(), ContentsThreadName.c_str());
        RT_ASSERT(!FAILED(hr));
    }

    //    std::map<CRingBuffer *, SRWLOCK> m_SrwMap;
    //    각 메세지 Q에 대해서 SRWLOCK을 획득하는 자료구조 초기화 하기.
    {
        balanceVec.reserve(ContentsThreadCnt);

        m_CotentsQ_vec.reserve(ContentsThreadCnt);
        hContentsThread_vec.reserve(ContentsThreadCnt);
        
        for (DWORD i = 0; i < ContentsThreadCnt; i++)
        {
            balanceVec.emplace_back(i, 0);
            m_CotentsQ_vec.emplace_back(s_ContentsQsize, 1);
            m_ContentsQMap[&m_CotentsQ_vec[i]];

            m_ContentsQMap[&m_CotentsQ_vec[i]].second = CreateEvent(nullptr, false, false, nullptr);
        }
        for (DWORD i = 0; i < ContentsThreadCnt; i++)
            InitializeSRWLock(&m_ContentsQMap[&m_CotentsQ_vec[i]].first);
    }


    // TODO : Sector마다  Lock 생성하기.
    for (DWORD w = 0; w < dfRANGE_MOVE_BOTTOM / dfSECTOR_Size; w++)
    {
        for (DWORD h = 0; h < dfRANGE_MOVE_BOTTOM / dfSECTOR_Size; h++)
        {
            InitializeSRWLock(&srw_Sectors[w][h]);
        }
    }

    m_ServerOffEvent = CreateEvent(nullptr, true, false, nullptr);

    player_pool.Initalize(m_maxPlayers);
    player_pool.Limite_Lock(); // Pool이 더 이상 늘어나지않음.
}

CTestServer::~CTestServer()
{
    pBalanceThread.join(); // Balance

    CloseHandle(pBalanceThread.native_handle());
    for (DWORD i = 0; i < m_ContentsThreadCnt; i++)
    {
        hContentsThread_vec[i].join();
        CloseHandle(hContentsThread_vec[i].native_handle());
    }
}

void CTestServer::Update()
{

    size_t addr;
    CMessage *msg;

    ringBufferSize  DeQSisze;
    ull l_sessionID;

    WORD type;

    CRingBuffer* CotentsQ = &m_CotentsQ_vec[tls_ContentsQIdx];
    // msg  크기 메세지 하나에 8Byte
    while (CotentsQ->GetUseSize() != 0)
    {
        CPlayer *player = nullptr;

        DeQSisze = CotentsQ->Dequeue(&addr, sizeof(size_t));
        if (DeQSisze != sizeof(size_t))
            __debugbreak();

        msg = (CMessage *)addr;
        l_sessionID = msg->ownerID;

        *msg >> type;

        // LoginPacket이 안오는 경우가 존재하지않음.

        { //  Client Message]
            AcquireSRWLockShared(&srw_SessionID_Hash);

            if (SessionID_hash.find(l_sessionID) != SessionID_hash.end())
                player = SessionID_hash[l_sessionID];
            else
            {
                stTlsObjectPool<CMessage>::Release(msg);
                ReleaseSRWLockShared(&srw_SessionID_Hash);
                continue;
            }
            ReleaseSRWLockShared(&srw_SessionID_Hash);
  
            InterlockedExchange(&player->m_Timer, timeGetTime());

            PacketProc(l_sessionID, msg, type);
           

            InterlockedIncrement64(&m_RecvMsgArr[type]);
        }
        
        InterlockedIncrement64(&m_UpdateTPS);
    }

}

void CTestServer::BalanceUpdate()
{
    size_t addr;
    CMessage *msg;

    ringBufferSize DeQSisze;
    ull l_sessionID;

    WORD type;

    CRingBuffer *CotentsQ = &m_BalanceQ;

    
    // msg  크기 메세지 하나에 8Byte
    while (CotentsQ->GetUseSize() != 0)
    {

        DeQSisze = CotentsQ->Dequeue(&addr, sizeof(size_t));
        if (DeQSisze != sizeof(size_t))
            __debugbreak();

        msg = (CMessage *)addr;
        l_sessionID = msg->ownerID;

        *msg >> type;

        // prePlayer의 증가, 삭제를 담당.
        // Player_hash의 증가 , 삭제를 담당.

        
        switch (type)
        {
        // 현재 미 사용중
        case en_PACKET_Player_Alloc:
            
            AllocPlayer(msg); // prePlayer의 증가.
            _InterlockedDecrement64(&m_NetworkMsgCount);
            break;
        // TODO : 끊김과 직전의 메세지가 처리가 되지않을 수 있음.  순서가 달라져서 
        // 이렇게 하면 PlayerHash와 prePlayerHash는 ContentsThread에서는 접근할 일이 없음. 

        case en_PACKET_Player_Delete:

            AcquireSRWLockExclusive(&srw_SessionID_Hash);

            DeletePlayer(msg);

            ReleaseSRWLockExclusive(&srw_SessionID_Hash);

            _InterlockedDecrement64(&m_NetworkMsgCount);
            break;
        case en_PACKET_CS_CHAT_REQ_LOGIN:

            AcquireSRWLockExclusive(&srw_SessionID_Hash);

                PacketProc(l_sessionID, msg, type);//Login만 실행.

            ReleaseSRWLockExclusive(&srw_SessionID_Hash);
            break;

        default:
            //TODO : 공격이 아닌상황에서 올수 없음.
            __debugbreak();
        }

    }

    DWORD currentTime,playerTime;
    DWORD msgInterval;

    currentTime = timeGetTime();
    //// LoginPacket을 대기하는 하트비트 부분
    {

  
        CPlayer *player;

        // prePlayer_hash 는 LoginPacket을 대기하는 Session
        for (auto &element : prePlayer_hash)
        {
            player = element.second;

            playerTime = player->m_Timer;

            msgInterval = currentTime - playerTime;
            if (msgInterval >= 5000 && playerTime < currentTime)
            {
                CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %05lld %12s %05llu %12s %05llu ",
                                               L"TimeOver_prePlayer : ", player->m_AccountNo,
                                               L"현재들어온ID:", player->m_sessionID);
                Disconnect(player->m_sessionID);
            }
        }
    }

    //// LoginPacket을 받아서 승격된 하트비트 부분
    {

        DWORD disTime;
        CPlayer *player;

        // AccountNo_hash 는 LoginPacket을 받아서 승격된 Player
        for (auto &element : AccountNo_hash)
        {
            player = element.second;

            playerTime = player->m_Timer;
            msgInterval = currentTime - playerTime;

            if (msgInterval >= 40000 && playerTime < currentTime)
            {
                CSystemLog::GetInstance()->Log(L"ContentsLog", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %05lld %12s %05llu %12s %05llu ",
                                               L"TimeOver_Player : ", player->m_AccountNo,
                                               L"현재들어온ID:", player->m_sessionID);
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

    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);
    RT_ASSERT(hMonitorThread != nullptr);
    hr = SetThreadDescription(hMonitorThread, L"\tMonitorThread");
    RT_ASSERT(!FAILED(hr));

    return retval;
}

float CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ )
{
    // double CurrentQ;
    ringBufferSize ContentsUseSize;
    CMessage **ppMsg;
    CRingBuffer *TargetQ; //  Q 가 많아졌으므로 Q를 찾아오기.
    HANDLE hMsgQueuedEvent; // Q에 데이터가 들어왔음을 알림.
    SRWLOCK *pSrw;
    
    std::wstring ProfileName; 

    if (bBalanceQ)
    {
        TargetQ = &m_BalanceQ; // Q랑 매핑되는 SRWLock을 획득하기.
        pSrw = &srw_BalanceQ;
        hMsgQueuedEvent = hBalanceEvent;
        ProfileName = L"EnQueue_BalanceQ";
    }
    else
    {
        AcquireSRWLockShared(&srw_SessionID_Hash);
        if (SessionID_hash.find(SessionID) == SessionID_hash.end())
        {
            TargetQ = &m_BalanceQ; // Q랑 매핑되는 SRWLock을 획득하기.
            pSrw = &srw_BalanceQ;
            hMsgQueuedEvent = hBalanceEvent;
            ProfileName = L"EnQueue_BalanceQ";
        }
        else
        {
            CPlayer *player = SessionID_hash[SessionID];

            TargetQ = &m_CotentsQ_vec[player->m_ContentsQIdx];
            pSrw = &m_ContentsQMap[TargetQ].first;
            hMsgQueuedEvent = m_ContentsQMap[TargetQ].second;
            ProfileName = L"EnQueue_CotentsQ" + std::to_wstring(player->m_ContentsQIdx);

               
        }
        ReleaseSRWLockShared(&srw_SessionID_Hash);
    }

        
    

    {

        stSRWLock srw(pSrw);
        Profiler profile;

        ppMsg = &msg;
        
        {
            profile.Start(ProfileName.c_str());
            // 내가 넣은 msg인데 여기에 없으면 ( X )
            if (TargetQ->Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
                __debugbreak();

            profile.End(ProfileName.c_str());
            SetEvent(hMsgQueuedEvent);
        }

        _interlockedincrement64(&m_RecvTPS);

        ContentsUseSize = TargetQ->GetUseSize();
    }
    return float(ContentsUseSize) / float(CTestServer::s_ContentsQsize) * 100.f;
}

bool CTestServer::OnAccept(ull SessionID)
{
    // Accept의 요청이 밀리고 있다고한다면, 그 이후에 Accept로 들어오는 Session을 끊겠다.
    // m_AllocMsgCount 처리량을 보여주는 변수.
    LONG64 localAllocCnt;
    clsSession &session = sessions_vec[SessionID >> 47];

    localAllocCnt = _InterlockedIncrement64(&m_AllocMsgCount); // Alloc Message가 너무 쏟아진다면 인큐를 안함.
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
        // TODO : 실패의 경우의 수
        OnRecv(SessionID, msg,true);

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

        OnRecv(SessionID, msg,true);

        _InterlockedIncrement64(&m_NetworkMsgCount);
    }
}

void MsgTypePrint(WORD type, LONG64 val)
{
    // 0 은 ㄴㄴ
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
