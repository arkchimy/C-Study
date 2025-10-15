//#include "pch.h"
#include "clsSession.h"
#include "../../../error_log.h"
#include "../CSystemLog_lib/CSystemLog_lib.h"

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
    if (InterlockedExchange(&m_blive,0) == 1)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"Release_socket_live  HANDLE value : %lld  seqID :%llu  seqIndx : %llu\n",
                                       m_sock, m_SeqID.SeqNumberAndIdx, m_SeqID.idx);
        return;
    }
    CMessage *msg;
    while (m_sendBuffer.Pop(msg))
    {
        delete msg;
    }
    
    m_recvBuffer.ClearBuffer();
}
