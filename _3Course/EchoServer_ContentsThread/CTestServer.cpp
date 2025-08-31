#include "stdafx.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "CTestServer.h"

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

    // bool �� ���ƺ���.
    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
            break;

        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;
        useSize = server->m_ContentsQ.GetUseSize(f, r);

        // ID + msg  ũ�� �޼��� �ϳ��� 16Byte
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
            // TODO : ��� Type�� �ִ´ٸ� Switch���� Ž.
            server->EchoProcedure(l_sessionID,message);
            f = server->m_ContentsQ._frontPtr;
            useSize -= 16;
        }
    }

    return 0;
}

unsigned MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};

    while (1)
    {
        WaitForMultipleObjects(2, hWaitHandle, false, 1000);
        server->OnRecv(1, nullptr);
    }

    return 0;
}

CTestServer::CTestServer()
{
    InitializeSRWLock(&srw_ContentQ);
    InitializeSRWLock(&srw_session_idleList);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);

    hContentsThread = (HANDLE)_beginthreadex(nullptr, 0, ContentsThread, this, 0, nullptr);
}

CTestServer::~CTestServer()
{
}

double CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    double CurrentQ;

    if (msg == nullptr)
    {
        CurrentQ = double(m_ContentsQ.GetUseSize()) / m_ContentsQ.s_BufferSize * 100.f;
        SetEvent(m_ContentsEvent);
        return CurrentQ;
    }

    {
        stSRWLock srw(&srw_ContentQ);
        // �����͸� �ְ�
        CMessage **ppMsg;
        ppMsg = &msg;

        if (m_ContentsQ.Enqueue(&sessionID, sizeof(sessionID)) != sizeof(sessionID))
            __debugbreak();

        if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
            __debugbreak();
        SetEvent(m_ContentsEvent);
    }

    return double(m_ContentsQ.GetUseSize()) / m_ContentsQ.s_BufferSize * 100.f;
}

void CTestServer::SendPostMessage(ull SessionID)
{
    clsSession *session;
    stSessionId stsessionid;
    stsessionid.SeqNumberAndIdx = SessionID;

    BOOL EnQSucess;
    {
        stSRWLock srw(&srw_session_idleList);

        session = &sessions_vec[stsessionid.idx];
        if (session->m_SeqID != stsessionid)
        {
            // TODO : discconect �� �״ٸ� �̻�Ȳ�� ����.
            __debugbreak();
            return;
        }
    }
    InterlockedIncrement(&session->m_ioCount);
    EnQSucess = PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_postOverlapped);
    if (EnQSucess == false)
    {
        InterlockedDecrement(&session->m_ioCount);
        ERROR_FILE_LOG(L"IOCP_ERROR.txt", L"PostQueuedCompletionStatus Failde");
    }
}

void CTestServer::RecvPostMessage(clsSession *session)
{
    BOOL EnQSucess;

    {
        ZeroMemory(&session->m_RecvpostOverlapped, sizeof(OVERLAPPED));
    }

    InterlockedIncrement((unsigned long long *)&session->m_ioCount);
    InterlockedIncrement((unsigned long long *)&session->m_RcvPostCnt);

    EnQSucess = PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_RecvpostOverlapped);
    if (EnQSucess == false)
    {
        InterlockedDecrement(&session->m_ioCount);
        ERROR_FILE_LOG(L"IOCP_ERROR.txt", L"PostQueuedCompletionStatus Failde");
    }
}

void CTestServer::EchoProcedure(ull sessionID, CMessage * message)
{
    stSessionId stMsgSessionID,currentSessionID;
    clsSession *session;

    stMsgSessionID.SeqNumberAndIdx = sessionID;

    session = &sessions_vec[stMsgSessionID.idx];
    currentSessionID = session->m_SeqID;

    //TODO : �ε����� ���� �ٸ� Session
    if (currentSessionID != stMsgSessionID)
    {
        delete message;
        return;
    }
    
    // TODO : disconnect�� ����ؼ� Ǯ�� ��������.
    // TODO : cs_sessionMap Lock ����
    {
        stSRWLock srw(&srw_session_idleList);
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
    if (InterlockedCompareExchange(&session->m_Postflag, 1, 0) == 0)
        SendPostMessage(stMsgSessionID.SeqNumberAndIdx);

}
