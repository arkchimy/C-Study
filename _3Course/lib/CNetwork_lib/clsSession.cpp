//#include "pch.h"
#include "clsSession.h"
#include "../../../error_log.h"
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
    if (InterlockedCompareExchange(&m_blive, false, true) == false)
    {
        ERROR_FILE_LOG(L"Critical_Error.txt", L"socket_live Change Failed");
    }

    m_sendBuffer.ClearBuffer();
    m_recvBuffer.ClearBuffer();

}
