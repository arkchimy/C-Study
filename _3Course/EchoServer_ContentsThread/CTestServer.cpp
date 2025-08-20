#include "stdafx.h"
#include "CTestServer.h"

#include <thread>

void EchoProcedure(CMessage *const message, CTestServer *const server)
{
    ull session_id;
    char payload[8];
    short len = 8;
    clsSession *session;
    *message >> session_id;
    message->GetData(payload, sizeof(payload));
    // TODO : disconnect에 대비해서 풀을 만들어야함.
  
    EnterCriticalSection(&server->cs_sessionMap);
    session = server->sessions[session_id];
    if (InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
    {
        session->m_sendBuffer->Enqueue(&len, sizeof(len));
        if (session->m_sendBuffer->Enqueue(payload, 8) != 8)
        {
            session->m_blive = false;
            LeaveCriticalSection(&server->cs_sessionMap);
            ERROR_FILE_LOG(L"session_Error.txt", L"(session->m_sendBuffer->Enqueue another");
            return;
        }
        server->SendPacket(session);
    }
    LeaveCriticalSection(&server->cs_sessionMap);
    delete message;
}
unsigned ContentsThread(void *arg)
{
    size_t addr;
    CMessage *message;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    //bool 이 좋아보임.
    while (1)
    {
        while (server->m_ContentsQ.GetUseSize() != 0)
        {

            if (server->m_ContentsQ.Dequeue(&addr, sizeof(size_t)) != sizeof(size_t))
                __debugbreak();
            message = (CMessage *)addr;
            // TODO : 헤더 Type을 넣는다면 Switch문을 탐.
            // switch ((*message)->)
            EchoProcedure(message,server);
        }
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
