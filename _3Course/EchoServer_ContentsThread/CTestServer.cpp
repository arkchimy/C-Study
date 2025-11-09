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

            //// TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            msg->GetData((char *)&header, server->headerSize);
            if (server->PacketProc(l_sessionID, msg, header) == false)
                server->Disconnect(l_sessionID);

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

bool CTestServer::EchoProcedure(ull SessionID, CMessage *msg, char *const buffer, short len)
{
    //해당 ID가 어느 섹터에 있는지, 보낼 대상을 특정하는 Logic
    this->Proxy::EchoProcedure(SessionID, msg, buffer,len);
    return true;
}
bool CTestServer::LoginProcedure(ull SessionID, CMessage *msg, INT64 AccontNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey)
{
    //idx동일.
    CPlayer &player = player_vec[SessionID >> 47];
    if (player.m_State != CPlayer::en_State::DisConnect)
        return false;
    player.m_State = CPlayer::en_State::IN_GAME;
    player.m_sessionID = SessionID;

    player.m_Timer = timeGetTime();
    player.m_AccountNo = AccontNo;

    memcpy(player.m_ID, ID, sizeof(player.m_ID));
    memcpy(player.m_Nickname, Nickname, sizeof(player.m_Nickname));
 

    Proxy::LoginProcedure(SessionID, msg, AccontNo);
    return true;

}

CTestServer::CTestServer(int iEncording)
    : CLanServer(iEncording)
{
    InitializeSRWLock(&srw_ContentQ);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);
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

    char *LoginPacket = CreateLoginMessage();

    WSABUF wsabuf;
    int send_retval;
    DWORD LastError;
    ull local_IoCount;

    clsSession &session = sessions_vec[ SessionID  >> 47];
    ull idx = SessionID  >> 47;


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
