#include "stdafx.h"
#include "CTestServer.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"


#include <thread>

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
    CMessage *message, *peekMessage;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    char *f, *r;
    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};
    DWORD hSignalIdx;
    ringBufferSize useSize, DeQSisze;
    ull l_sessionID;

    // bool 이 좋아보임.

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
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
        while (useSize >= 8)
        {

            DeQSisze = server->m_ContentsQ.Dequeue(&addr, sizeof(size_t));
            if (DeQSisze != sizeof(size_t))
                __debugbreak();

            message = (CMessage *)addr;
            l_sessionID = message->ownerID;

            //// TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            server->PacketProc(l_sessionID, message);

            f = server->m_ContentsQ._frontPtr;
            useSize -= 8;
        }
    }

    return 0;
}

bool CTestServer::PacketProc(ull SessionID, CMessage *msg)
{
    // TODO : 추후에  Code로 Case문 바꾸기.
    //  메세지를 보낼때,

    float qPersentage;
    short len;
    char EchoBuffer[100];
    stHeader header;
    msg->GetData((char *)&header, headerSize);
    if (bEnCording)
        len = header._len;
    else
        len = 8;

    // TODO : Header.Len => Code로 바꾸기
    switch (len)
    {
    case 8:
    {
 
        // 헤더를 그대로 복사.
        /*
         * msg << 이런식 사용.
         */
        msg->GetData(EchoBuffer, header._len);
        header._len = 8;
        stTlsObjectPool<CMessage>::Release(msg);
        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

        msg->PutData((const char *)&header, sizeof(stHeader));
        msg->PutData(EchoBuffer, header._len);
        EchoProcedure(SessionID, msg);
    }
    break;
    default:
        //TODO : 이부분 다시 보기
        msg->GetData(EchoBuffer, header._len);
        stTlsObjectPool<CMessage>::Release(msg);

        msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();

        msg->PutData((const char *)&header, sizeof(stEnCordingHeader));
        msg->PutData(EchoBuffer, sizeof(EchoBuffer));
        EchoProcedure(SessionID, msg);
    }

    return true;
}

void CTestServer::EchoProcedure(ull SessionID, CMessage *message)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;
    ull seqID;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        // 새로운 세션으로 초기화되지않았고, r_flag가 세워져있으면 진입하지말자.
        stTlsObjectPool<CMessage>::Release(message);
        // 이미 r_flag가 올라가있는데 IoCount를 잘못 올린다고 문제가 되지않을 것같다.
        return;
    }

    // session의 Release는 막았으므로 변경되지않음.
    seqID = session.m_SeqID.SeqNumberAndIdx;
    if (seqID != SessionID)
    {
        // 새로 세팅된 Session이므로 다른 연결이라 판단.
        stTlsObjectPool<CMessage>::Release(message);
        // 내가 잘못 올린 ioCount를 감소 시켜주어야한다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(seqID);

        return;
    }

    if (Local_ioCount == 1)
    {
        // 원래 '0'이 었는데 내가 증가시켰다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(SessionID);

        stTlsObjectPool<CMessage>::Release(message);
        return;
    }

    // 여기까지 왔다면, 같은 Session으로 판단하자.
    CMessage **ppMsg;
    ull local_IoCount;
    ppMsg = &message;

    if (bEnCording)
        (*ppMsg)->EnCording();
    {
        Profiler profile;
        profile.Start(L"LFQ_Push");

        session.m_sendBuffer.Push(*ppMsg);
        profile.End(L"LFQ_Push");
    }

    // PQCS를 시도.
    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
        local_IoCount = InterlockedIncrement(&session.m_ioCount);

        PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
    }

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
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
    static ll visited = 0;
    ringBufferSize ContentsUseSize;

    stSRWLock srw(&srw_ContentQ);

    ContentsUseSize = m_ContentsQ.GetUseSize();
    if (msg == nullptr)
    {

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

