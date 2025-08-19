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
    //�����͸� �ְ�
    if (m_ContentsQ.Enqueue(msg, sizeof(msg)) != sizeof(msg))
        __debugbreak();
    LeaveCriticalSection(&cs_ContentQ);

    return double(m_ContentsQ.GetUseSize()) / 1000.f * 100.f;
}
