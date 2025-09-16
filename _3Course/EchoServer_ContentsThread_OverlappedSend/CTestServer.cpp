#include "stdafx.h"
#include "../lib/CNetwork_lib_OverlappedSend/CNetwork_lib.h"
#include "CTestServer.h"

#include <thread>
extern SRWLOCK srw_Log;
unsigned SendThread(void *arg);
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

    HANDLE hWaitHandle = { server->m_ServerOffEvent};

    DWORD wait_retval;
    LONG64 beforeTotal = 0;

    double cnt = 0;
    double avg;
    while (1)
    {
        cnt++;
        wait_retval = WaitForSingleObject( hWaitHandle, 1000);

        if (beforeTotal == server->m_TotalTPS)
        {
            cnt--;
            continue;
        }
        if (wait_retval != WAIT_OBJECT_0)
        {
            beforeTotal = server->m_TotalTPS;
            avg = beforeTotal / cnt;
            
            printf("==================================\n");
            printf("Avg  Send TPS : %llf\n", avg);
            printf("==================================\n");
        }
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
    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);
    
    _beginthreadex(nullptr, 0, SendThread, this, 0, nullptr);
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
        // 포인터를 넣고
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

bool CTestServer::OnAccept(ull SessionID, SOCKADDR_IN addr)
{
    stHeader header;
    clsSession *session;
    stSessionId currentID;
    char buffer[] = {0x08, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};

    header._len = 8;


    currentID.SeqNumberAndIdx = SessionID;
    session = &sessions_vec[currentID.idx];
    
    if (currentID != session->m_SeqID)
        return false;

    send(session->m_sock, buffer, 10, 0);
    return true;
}

HANDLE hSendThreadIOCP;

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

    //TODO : 인덱스만 같은 다른 Session
    if (currentSessionID != stMsgSessionID)
    {
        CObjectPoolManager::pool.Release(message);
        return;
    }
    
    // TODO : disconnect에 대비해서 풀을 만들어야함.
    // TODO : cs_sessionMap Lock 제거
  
    stSendOverlapped *overlapped = new stSendOverlapped();
    overlapped->_msg = message;
    overlapped->_mode = Job_Type::Send;

    PostQueuedCompletionStatus(hSendThreadIOCP, 0, (ULONG_PTR)session, overlapped);
}


unsigned SendThread(void *arg)
{

    DWORD transferred;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    ull key;

    stSendOverlapped *overlapped;
    clsSession *session;

    ull local_ioCount;
    hSendThreadIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, 1 );
    WSABUF wsabuf;

    while (1)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        GetQueuedCompletionStatus(hSendThreadIOCP, &transferred, &key, (LPOVERLAPPED*)&overlapped, INFINITE);

        if (overlapped == nullptr)
            __debugbreak();
        session = reinterpret_cast<clsSession *>(key);

        switch (overlapped->_mode)
        {
        case Job_Type::Send:
            wsabuf.buf = overlapped->_msg->_frontPtr;
            wsabuf.len = overlapped->_msg->_rearPtr - overlapped->_msg->_frontPtr;
            overlapped->_mode = Job_Type::Recv;
            server->SendPacket(session, overlapped->_msg);
            server->m_TotalTPS++;

            PostQueuedCompletionStatus(hSendThreadIOCP, 0, (ULONG_PTR)session, overlapped);
            break;
        case Job_Type::Recv:

            delete overlapped;
            break;
        }


    }
    return 0;
}
