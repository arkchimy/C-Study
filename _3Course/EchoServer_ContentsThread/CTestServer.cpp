#include "stdafx.h"
#include "CTestServer.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include <thread>
extern SRWLOCK srw_Log;

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
            // TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            server->EchoProcedure(l_sessionID, message);
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
            int sessionCnt = 0;
            for (auto &element : server->sessions_vec)
            {
                if (element.m_blive)
                    sessionCnt++;
            }
            printf(" Total Sessions: %d\n", sessionCnt);
            for (int i = 0; i <= server->m_WorkThreadCnt; i++)
            {
                if (i == 0)
                    printf(" Conetents Send TPS : %lld\n", arrTPS[i]);
                else
                    printf(" Send TPS : %lld\n", arrTPS[i]);
            }
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

    // stHeader header;
    clsSession *session;
    stSessionId currentSessionID;
    WSABUF wsabuf;
    int send_retval;
    DWORD LastError;

    currentSessionID.SeqNumberAndIdx = SessionID;
    session = &sessions_vec[currentSessionID.idx];

    if (currentSessionID != session->m_SeqID)
    {
        ERROR_FILE_LOG(L"Critical_Error.txt", L"currentID != session->m_SeqID");
        return false;
    }

    // send_retval = send(session->m_sock, LoginPacket, sizeof(LoginPacket), 0);
    // if (send_retval <= 0)
    //{
    //     return false;
    // }

    wsabuf.buf = LoginPacket;
    wsabuf.len = 10;

    _InterlockedExchange(&session->m_flag, 1);

    ull local_IoCount = InterlockedIncrement(&session->m_ioCount);

    // wchar_t buffer[1000];
    // StringCchPrintfW(buffer, sizeof(buffer) / sizeof(wchar_t), L" sessionPrt : %p  , sessionIdx : %lld  ,sock : %llu , OnAcceptSend  ioCount %llu ", session, currentSessionID.idx,  session->m_sock, local_IoCount);
    // ERROR_FILE_LOG(L"SystemLog.txt", buffer);

    ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));

    send_retval = WSASend(session->m_sock, &wsabuf, 1, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
    LastError = GetLastError();

    if (send_retval < 0)
    {
        if (LastError != WSA_IO_PENDING)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"WSASend Error ");
            InterlockedDecrement(&session->m_ioCount);
        }
    }

    /*
    *     CMessage *msg = CreateLoginMessage();
    CMessage **ppMsg;
    ppMsg  = &msg;

    session->m_sendBuffer.ClearBuffer();
    if (session->m_sendBuffer.Enqueue(ppMsg, sizeof(size_t)) != sizeof(size_t))
    {
        session->m_blive = false;
        ERROR_FILE_LOG(L"session_Error.txt", L"(session->m_sendBuffer.Enqueue another");
        __debugbreak();
        return false;
    }*/
    return true;
}

void CTestServer::EchoProcedure(ull sessionID, CMessage *message)
{
    clsSession *session;

    session = &sessions_vec[sessionID & 0xffff];
    // TODO : 인덱스만 같은 다른 Session
    if (sessionID != session->m_SeqID.SeqNumberAndIdx)
    {
        CObjectPoolManager::pool.Release(message);
        return;
    }

    // TODO : disconnect에 대비해서 풀을 만들어야함.
    //
    if (InterlockedExchange(&session->m_Useflag, 1) != 0)
        return;
    if (InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0) == 0)
    {
        session->Release();
        return;
    }

    if ((session->m_ioCount & (ull)1 << 47) != 0)
    {
        // Release Flag가 켜져있다면,
        session->Release();
        return;
    }

    {

        CMessage **ppMsg;
        ppMsg = &message;

        if (session->m_sendBuffer.Enqueue(ppMsg, sizeof(size_t)) != sizeof(size_t))
        {
            session->m_blive = false;
            ERROR_FILE_LOG(L"session_Error.txt", L"(session->m_sendBuffer.Enqueue another");
            __debugbreak();
            return;
        }
    }

    if (InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
        ull local_IoCount = InterlockedIncrement(&session->m_ioCount);

        PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_sendOverlapped);
    }
    if (InterlockedExchange(&session->m_Useflag, 0) == 2)
    {
        session->Release();
    }
}
