#include "stdafx.h"
#include "CTestServer.h"

#include <thread>

unsigned ContentsThread(void *arg)
{
    size_t addr;
    CMessage *message;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    char *f, *r;


    //bool 이 좋아보임.
    while (1)
    {
        EnterCriticalSection(&server->cs_ContentQ);

        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;
        while (server->m_ContentsQ.GetUseSize(f,r) != 0)
        {
            if (server->m_ContentsQ.Peek(&addr, sizeof(size_t), f, r) != sizeof(size_t))
                __debugbreak();
            message = (CMessage *)addr;
            message->_begin = message->_begin; 
            if (server->m_ContentsQ.Dequeue(&addr, sizeof(size_t),f,r) != sizeof(size_t))
                __debugbreak();
            f = server->m_ContentsQ._frontPtr;
         
            
            // TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            server->EchoProcedure(message);
        }
        LeaveCriticalSection(&server->cs_ContentQ);
        Sleep(20);
    }

    return 0;
}

CTestServer::CTestServer()
{
    InitializeCriticalSection(&cs_ContentQ);
    InitializeCriticalSection(&cs_sessionMap);
    hContentsThread = (HANDLE)_beginthreadex(nullptr, 0, ContentsThread, this, 0, nullptr);

}

CTestServer::~CTestServer()
{
    DeleteCriticalSection(&cs_ContentQ);
    DeleteCriticalSection(&cs_sessionMap);
    
}

double CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    EnterCriticalSection(&cs_ContentQ);
    // 포인터를 넣고
    CMessage **ppMsg = &msg;

    if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
        __debugbreak();
    LeaveCriticalSection(&cs_ContentQ);

    return double(m_ContentsQ.GetUseSize()) / 1000.f * 100.f;
}

void CTestServer::SendPostMessage(ull SessionID)
{
    clsSession *session;
    BOOL EnQSucess;

    //TODO : cs_sessionMap Lock 제거
    EnterCriticalSection(&cs_sessionMap);
    session = sessions[SessionID];
    LeaveCriticalSection(&cs_sessionMap);

    InterlockedIncrement(&session->m_ioCount);
    EnQSucess = PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_postOverlapped);
    if (EnQSucess == false)
        InterlockedDecrement(&session->m_ioCount);

}

void CTestServer::EchoProcedure(CMessage *const message)
{
    ull session_id;
    char payload[8];
    short len = 8;
    clsSession *session;
    *message >> session_id;
    message->GetData(payload, sizeof(payload));
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

    if (session->m_sendBuffer->Enqueue(&len, sizeof(len)) != sizeof(len))
    {
        session->m_blive = false;
        LeaveCriticalSection(&cs_sessionMap);
        ERROR_FILE_LOG(L"session_Error.txt", L"(session->m_sendBuffer->Enqueue another");
        __debugbreak();
        return;
    }
    if (session->m_sendBuffer->Enqueue(payload, 8) != 8)
    {
        session->m_blive = false;
        LeaveCriticalSection(&cs_sessionMap);
        ERROR_FILE_LOG(L"session_Error.txt", L"(session->m_sendBuffer->Enqueue another");
        __debugbreak();
        return;
    }
    if (InterlockedCompareExchange(&session->m_Postflag, 1, 0) == 0)
        SendPostMessage(session_id);
    

    LeaveCriticalSection(&cs_sessionMap);
    delete message;

}
