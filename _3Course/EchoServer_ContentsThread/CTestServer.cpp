#include "stdafx.h"
#include    "CTestServer.h"

CTestServer::CTestServer()
{
    InitializeCriticalSection(&cs_ContentQ);
}

CTestServer::~CTestServer()
{
    DeleteCriticalSection(&cs_ContentQ);
}

double CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    EnterCriticalSection(&cs_ContentQ);
    //포인터를 넣고
    if (m_ContentsQ.Enqueue(msg, sizeof(msg)) != sizeof(msg))
        __debugbreak();
    LeaveCriticalSection(&cs_ContentQ);

    return double(m_ContentsQ.GetUseSize()) / 1000.f * 100.f;
}
