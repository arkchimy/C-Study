#include "clsSession.h"

#include "../utility/CSystemLog/CSystemLog.h"
#include "../utility/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../utility/CTlsObjectPool/CTlsObjectPool.h"

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
    for (DWORD i = 0; i < m_sendOverlapped.msgCnt; i++)
    {
        stTlsObjectPool<CMessage>::Release(m_sendOverlapped.msgs[i]);
    }
    m_sendOverlapped.msgCnt = 0;
    {
        ZeroMemory(&m_recvOverlapped, sizeof(OVERLAPPED));
        ZeroMemory(&m_sendOverlapped, sizeof(OVERLAPPED));
    }
    m_recvBuffer.ClearBuffer();
}
