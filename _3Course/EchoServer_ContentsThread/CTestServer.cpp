#include "stdafx.h"
#include "CTestServer.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"


#include <thread>

extern SRWLOCK srw_Log;
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

unsigned ContentsThread(void *arg)
{
    size_t addr;
    CMessage *message, *peekMessage;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    char *f, *r;
    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};
    DWORD hSignalIdx;
    ringBufferSize useSize, DeQSisze;
    ull l_sessionID;

    // bool 이 좋아보임.

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-20s ",
                                   L"This is ContentsThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is ContentsThread");
    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
            break;

        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;
        useSize = server->m_ContentsQ.GetUseSize(f, r);

        // ID + msg  크기 메세지 하나에 16Byte
        while (useSize >= 16)
        {

            DeQSisze = server->m_ContentsQ.Dequeue(&l_sessionID, sizeof(ull));
            if (DeQSisze != sizeof(ull))
                __debugbreak();

            DeQSisze = server->m_ContentsQ.Peek(&addr, sizeof(size_t));
            peekMessage = (CMessage *)addr;
            peekMessage->_begin = peekMessage->_begin;

            DeQSisze = server->m_ContentsQ.Dequeue(&addr, sizeof(size_t));
            if (DeQSisze != sizeof(size_t))
                __debugbreak();
            message = (CMessage *)addr;

            if (rand() % 1000 == 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                               L"%-10s %10s %012llu ",
                                               L"DisConnect_Event",
                                               L"seqID :", l_sessionID);
                if(server->DisconnectForContents(l_sessionID) == true)
                    stTlsObjectPool<CMessage>::Release(message);
                else
                    server->EchoProcedure(l_sessionID, message);
            }
            // TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            else
            {
                server->EchoProcedure(l_sessionID, message);
            }
            
            f = server->m_ContentsQ._frontPtr;
            useSize -= 16;
        }
    }

    return 0;
}

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

            if (server->GetSessionCount() == 0)
                continue;
            printf(" ==================================\nTotal Sessions: %d\n", server->GetSessionCount());
            printf(" Total iDisCounnectCount: %llu\n", server->iDisCounnectCount);
            printf(" IdxStack Size: %lld\n", server->Get_IdxStack());
            printf(" ReleaseSessions Size: %lld\n", server->GetReleaseSessions());

            for (int i = 1; i <= server->m_WorkThreadCnt; i++)
            {
                printf(" Send TPS : %lld\n", arrTPS[i]);
            }
            printf(" ==================================\n");
        }
    }

    return 0;
}

CTestServer::CTestServer()

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

double CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    // double CurrentQ;
    static ll visited = 0;
    ringBufferSize ContentsUseSize;

    stSRWLock srw(&srw_ContentQ);

    ContentsUseSize = m_ContentsQ.GetUseSize();
    if (msg == nullptr)
    {
        SetEvent(m_ContentsEvent);
        return double(ContentsUseSize) / double(CTestServer::s_ContentsQsize) * 100.f;
    }

    {
        Profiler profile;

        profile.Start(L"ContentsQ_Enqueue");

        // 포인터를 넣고
        CMessage **ppMsg;
        ppMsg = &msg;

        if (m_ContentsQ.Enqueue(&sessionID, sizeof(sessionID)) != sizeof(sessionID))
            __debugbreak();

        if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
            __debugbreak();

        profile.End(L"ContentsQ_Enqueue");
        SetEvent(m_ContentsEvent);
    }

    return double(ContentsUseSize) / double(CTestServer::s_ContentsQsize) * 100.f;
}

bool CTestServer::OnAccept(ull SessionID)
{

    static char *LoginPacket = CreateLoginMessage();

    WSABUF wsabuf;
    int send_retval;
    DWORD LastError;
    ull local_IoCount;

    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    ull idx = (SessionID & SESSION_IDX_MASK) >> 47;
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
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"WSASend",
                                   L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                   L"IO_Count", local_IoCount);
    LastError = GetLastError();

    if (send_retval < 0)
    {
        WSASendError(LastError, SessionID);
    }

    return true;
}

void CTestServer::EchoProcedure(ull sessionID, CMessage *message)
{
    ull SeqID;
    clsSession &session = sessions_vec[ sessionID >> 47];
    ull exchagRetval;

    // TODO : disconnect에 대비해서 풀을 만들어야함.
    //
    // 

    if (InterlockedCompareExchange(&session.m_Useflag, 1, 0) != 0)
    {
        // 이미 연결이 끊긴 세션의 메세지일 수 있음.
        // 연결이 끊긴상태면 '0'이 아닌값을 의미하므로 만일 '0'이 아닌 세션이라면 끊겨있는 세션으로 판단하여 무시.

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld %10s %05lld  %10s %012llu  %10s %4llu",
                                       L"EchoProcedure",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        stTlsObjectPool<CMessage>::Release(message);

        return;
    }
    // 지금 막 연결된  대상은 IOCount가 '1'임  만일 다른 ID의 세션으로 오인되도 제대로 끊기가능.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContetnsThread",
                                       L"1<<47 : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);

        //여기서는 현재 session이 IO가 0이므로 현재 session을 끊어야한다.
        SeqID = session.m_SeqID.SeqNumberAndIdx;
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContentsRelease1",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        stTlsObjectPool<CMessage>::Release(message);
        ReleaseSession(SeqID);
        return;
    }
    // TODO : 인덱스만 같은 다른 Session
    if (sessionID != session.m_SeqID.SeqNumberAndIdx)
    {
        // ★
        // CMessagePoolManager::pool.Release(message);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %4llu  %10s %3llu",
                                       L"idxCompareDiff",
                                       L"HANDLE : ", session.m_sock,
                                       L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Current_seqID :", sessionID,
                                       L"IO_Count", session.m_ioCount);

        if (InterlockedCompareExchange(&session.m_Useflag, 0,1) == 2)
        {
            //그사이에 WorkerThread가 Release 절차를 밟았다면. 여기서 해제.
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %d",
                                           L"EchoProcedure2",
                                           L"UseFlag : ", 0);

            SeqID = session.m_SeqID.SeqNumberAndIdx;
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"ContentsRelease5",
                                           L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                           L"IO_Count", session.m_ioCount);
        
            ReleaseSession(SeqID);
        }
        stTlsObjectPool<CMessage>::Release(message);
        return;
    }

    //_interlockedbittestandreset64
    //if ((session.m_ioCount & (ull)1 << 47) != 0)
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
    {
        // 뜨는 경우가 있을까?
        // 떳음.
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContetnsThread2",
                                       L"1<<47 : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        //어차피 Use로 물고있기 때문에 다른 연결이진않을듯?
        stTlsObjectPool<CMessage>::Release(message);
        ReleaseSession(sessionID);
        return;
    }
    CMessage **ppMsg;
    ull local_IoCount;
    ppMsg = &message;
    {
        {
            Profiler profile;
            profile.Start(L"LFQ_Push");

            session.m_sendBuffer.Push(*ppMsg);
            profile.End(L"LFQ_Push");
        }
    }

    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
        local_IoCount = InterlockedIncrement(&session.m_ioCount);
        if ((local_IoCount & (ull)1 << 47) == 0 && local_IoCount != 1)
        {
            PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
        }
        else
        {
            local_IoCount = InterlockedDecrement(&session.m_ioCount);
            if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                               L"ContetnsThread3",
                                               L"1<<47 : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"IO_Count", session.m_ioCount);

                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                               L"ContentsRelease4",
                                               L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"IO_Count", session.m_ioCount);

                ReleaseSession(sessionID);
                return;
            }
        }
    }

    if (InterlockedCompareExchange(&session.m_Useflag, 0,1) == 2)
    {

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContentsRelease3",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        ReleaseSession(sessionID);
    }

}
