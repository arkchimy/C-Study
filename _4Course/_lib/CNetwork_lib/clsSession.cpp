//#include "pch.h"
#include "clsSession.h"

#include "../CSystemLog_lib/CSystemLog_lib.h"
#include "../CLockFreeQueue_lib/CLockFreeQueue_lib.h"

extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

clsSession::clsSession(SOCKET sock)
    : m_sock(sock)
{
    m_blive = 1;
}

clsSession::~clsSession()
{
    closesocket(m_sock);
    __debugbreak();
}

void clsSession::Release()
{
    CMessage *msg;
    while (m_sendBuffer.Pop(msg))
    {
        stTlsObjectPool<CMessage>::Release(msg);
    }
    while (m_SendMsg.Pop(msg))
    {
        stTlsObjectPool<CMessage>::Release(msg);
    }
    m_recvBuffer.ClearBuffer();
}
