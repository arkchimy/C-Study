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

            printf(" ==================================\nTotal Sessions: %lld\n", server->GetSessionCount());
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
            case en_PACKET_Player_Alloc:
                server->InitalizePlayer(msg);
                break;
            
            default:
                server->PacketProc(l_sessionID, msg, header,type);
                 
            }

            f = server->m_ContentsQ._frontPtr;
            useSize -= 8;
        }
        //// 하트비트 부분
        //{
        //    DWORD currentTime;
        //    DWORD disTime;
        //    currentTime = timeGetTime();
        //    for (CPlayer &player : server->player_vec)
        //    {
        //        if (player.m_State == CPlayer::en_State::DisConnect)
        //            continue;
        //        disTime = currentTime - player.m_Timer;
        //        if (disTime >= 4000)
        //        {
        //            server->Disconnect(player.m_sessionID);
        //        }
        //    }
        //}
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

    clsSession &session = sessions_vec[SessionID >> 47];
    CPlayer *player;

    // 옳바른 연결인지는 SessionKey에 의존.
    {
        //Logic
    }

    //여기로 까지 왔다는 것은 로그인 서버의 인증을 통해 온  옳바른 연결이다.
    // 릴리즈 되었거나, ID가 다르다면 false를 리턴
    // 
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    {
        if (session.m_State != en_State::Session)
        {
            // 중복 로그인 패킷을 보낸 경우.
            stTlsObjectPool<CMessage>::Release(msg);
            Disconnect(SessionID);
            return;
        }
        
        session.m_State = en_State::Player;
        if (player_hash.find(AccountNo) != player_hash.end())
        {
            // char	SessionKey[64];		// 인증토큰
            // 같은 AccountNo가 로그인 되어있다. 
            // 누군가 자신이 아닌
            stTlsObjectPool<CMessage>::Release(msg);
            Disconnect(SessionID);
            Disconnect(player_hash[AccountNo]->m_sessionID);
            return;
        }
        //
        player = (CPlayer *)player_pool.Alloc();
        player_hash[AccountNo] = player;
        //Login응답.
        Proxy::LoginProcedure(SessionID, msg, AccountNo);
    }
   
    SessionUnLock(SessionID);
}

void CTestServer::InitalizePlayer(CMessage *msg)
{
    //Player생성 고민
    {
        // Hash의 key를 Account로 작성을 해야. 중복 로그인을 막을 수 있지않을까?
        //
        // 내가 만든 메세지임.
        // 왜냐하면 이 메세지는 디코딩을 안하고 사용함.
        // 원래 남이 내 메세지인척 들어온다면, 수신 버퍼에서는 무조건 꺼내고 디코딩하는 작업이 들어가는데.
        // 이 메세지는 디코딩을 안하고 바로 ContentsQ에 넣어줌으로써 나만이 사용할 수 있음.
    }
    CPlayer *player;

    INT64 AccountNo;
    *msg >> AccountNo;

    if (player_hash.find(AccountNo) == player_hash.end())
    {
        player = (CPlayer *)player_pool.Alloc();
        player_hash[AccountNo] = player; // 생성

        player_hash[AccountNo]->m_sessionID = msg->ownerID;
        player_hash[AccountNo]->m_State = en_State::Session;
        player_hash[AccountNo]->m_Timer = timeGetTime();
    }
    //만일 누군가 들어있다고 해도 이것을  Connect 단계에서 끊는 것은 잘못됨.

/* 핵심:

    해당연결에 대한 끊음은 ?
    연결 끊음을 session에 대해서 순회하자? 에반데.

    겹칠 경우 Player자료형을 따로 하나 더 두자? 
    아직 Player는 아니지만 , 연결을 끊기위한 
        
*/
    
}

CTestServer::CTestServer(int iEncording)
    : CLanServer(iEncording)
{
    InitializeSRWLock(&srw_ContentQ);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);

    for (int i = 0; i < m_maxSessions; i++)
        idle_stack.push(i);
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

    char *LoginPacket;
    int send_retval;
    DWORD LastError;
    ull local_IoCount;

    clsSession &session = sessions_vec[ SessionID  >> 47];
    ull idx = SessionID  >> 47;
    WSABUF wsabuf;
    
    // ContentsQ 에 PlayerAlloc 삽입 안하는 방향으로 가보는 중
   /* {
        CMessage *msg;
        stHeader header;
        short *ptr;
        header.byType = en_PACKET_Player_Alloc;
        
        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
        msg->PutData(&header, headerSize);
        *msg << SessionID;
        ptr = (short*)(msg->_frontPtr + offsetof(stHeader, sDataLen));
        *ptr = sizeof(SessionID);
        {
            stSRWLock lock(&srw_ContentQ);

            CMessage **ppMsg;
            ppMsg = &msg;
            (*ppMsg)->ownerID = SessionID;

            if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
                __debugbreak();

            SetEvent(m_ContentsEvent);


        }
       
    }*/
    session.m_State = en_State::Session;
    session.m_AcceptTime = timeGetTime(); //이때 시간을 저장.

    {

        LoginPacket = CreateLoginMessage();
        wsabuf.buf = LoginPacket;
        wsabuf.len = 10;

        _InterlockedExchange(&session.m_flag, 1);

        local_IoCount = InterlockedIncrement(&session.m_ioCount);

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"OnAccept",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);

        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

        send_retval = WSASend(session.m_sock, &wsabuf, 1, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
        LastError = GetLastError();

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"WSASend",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        if (send_retval < 0)
        {
            WSASendError(LastError, SessionID);
        }

        return true;
    }
}

bool CTestServer::GetId(short &idx)
{
    if (idle_stack.empty())
        return false;
    idx = idle_stack.top();
    idle_stack.pop();

    return true;
}
