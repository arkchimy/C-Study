//#include "pch.h"
#include "clsSession.h"
#include "../../../error_log.h"
#include "../CSystemLog_lib/CSystemLog_lib.h"
#include "../SerializeBuffer_exception/SerializeBuffer_exception.h"

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
    while (m_SendMsg.size() != 0)
    {
        msg = m_SendMsg.front();
        m_SendMsg.pop_front();
        stTlsObjectPool<CMessage>::Release(msg);
    }
    m_recvBuffer.ClearBuffer();
}
