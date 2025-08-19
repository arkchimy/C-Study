//#include "pch.h"
#include "clsSession.h"

clsSession::clsSession(SOCKET sock)
    : m_sock(sock)
{

    m_sendBuffer = new CRingBuffer();
    m_recvBuffer = new CRingBuffer();
    m_blive = 1;
}

clsSession::~clsSession()
{
    delete m_sendBuffer;
    delete m_recvBuffer;

    closesocket(m_sock);
}

void clsSession::Release()
{
    m_blive = 0;
}
