#include "stdafx.h"
#include "CTestServer.h"
#include <thread>

unsigned ContentsThread(void *arg)
{
    size_t addr;
    CMessage *message;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    char *f, *r;
    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};
    DWORD hSignalIdx;

    // bool 이 좋아보임.
    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
            break;

        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;

        while (server->m_ContentsQ.GetUseSize(f, r) >= 8)
        {
            if (server->m_ContentsQ.Dequeue(&addr, sizeof(size_t), f, r) != sizeof(size_t))
                __debugbreak();
            message = (CMessage *)addr;
            // TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            server->EchoProcedure(message);
            f = server->m_ContentsQ._frontPtr;
        }
    }

    return 0;
}

CTestServer::CTestServer()
{
    InitializeCriticalSection(&cs_ContentQ);
    InitializeCriticalSection(&cs_sessionMap);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);

    hContentsThread = (HANDLE)_beginthreadex(nullptr, 0, ContentsThread, this, 0, nullptr);
}

CTestServer::~CTestServer()
{
    DeleteCriticalSection(&cs_ContentQ);
    DeleteCriticalSection(&cs_sessionMap);
}

double CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    double CurrentQ;

    if (msg == nullptr)
    {
        CurrentQ = double(m_ContentsQ.GetUseSize()) / 1000.f * 100.f;
        return CurrentQ;
    }

    EnterCriticalSection(&cs_ContentQ);
    // 포인터를 넣고
    CMessage **ppMsg;
    ppMsg = &msg;

    if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
        __debugbreak();
    SetEvent(m_ContentsEvent);
    LeaveCriticalSection(&cs_ContentQ);

    return double(m_ContentsQ.GetUseSize()) / 1000.f * 100.f;
}

void CTestServer::SendPostMessage(ull SessionID)
{
    clsSession *session;
    BOOL EnQSucess;

    session = sessions[SessionID];

    InterlockedIncrement(&session->m_ioCount);
    EnQSucess = PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_postOverlapped);
    if (EnQSucess == false)
    {
        InterlockedDecrement(&session->m_ioCount);
        ERROR_FILE_LOG(L"IOCP_ERROR.txt", L"PostQueuedCompletionStatus Failde");
    }
}

void CTestServer::EchoProcedure(CMessage *const message)
{
    ull session_id;
    char payload[8];
    short len = 8;
    clsSession *session;
    char *payloadoffset;
    char messageData[10];

    memcpy(messageData, &len, sizeof(len));
    payloadoffset = messageData + sizeof(len);
    try
    {
        *message >> session_id;
        message->GetData(payloadoffset, len);
    }
    catch (MessageException::ErrorType err)
    {
        switch (err)
        {
        case MessageException::HasNotData:
            __debugbreak();
            break;
        }
    }

    // TODO : disconnect에 대비해서 풀을 만들어야함.
    // TODO : cs_sessionMap Lock 제거

    EnterCriticalSection(&cs_sessionMap);
    auto iter = sessions.find(session_id);
    if (iter == sessions.end())
    {
        LeaveCriticalSection(&cs_sessionMap);
        __debugbreak();
        return;
    }
    session = iter->second;
    LeaveCriticalSection(&cs_sessionMap);

    if (session->m_sendBuffer->Enqueue(messageData, sizeof(messageData)) != sizeof(messageData))
    {
        session->m_blive = false;
        ERROR_FILE_LOG(L"session_Error.txt", L"(session->m_sendBuffer->Enqueue another");
        __debugbreak();
        return;
    }

    if (InterlockedCompareExchange(&session->m_Postflag, 1, 0) == 0)
        SendPostMessage(session_id);

    delete message;
}
