//#include "pch.h"
#include "clsSession.h"

clsSession::clsSession(SOCKET sock)
    : m_sock(sock)
{
    m_blive = 1;
}

clsSession::~clsSession()
{
    closesocket(m_sock);
}

void clsSession::Release()
{
    m_blive = 0;
}
