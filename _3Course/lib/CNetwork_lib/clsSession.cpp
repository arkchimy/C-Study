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
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Release ",
                                       L"HANDLE : ", m_sock, L"seqID :", m_SeqID.SeqNumberAndIdx, L"seqIndx : ", m_SeqID.idx,
                                       L"IO_Count", m_ioCount);

        return;
    }
    else
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Release Fail",
                                       L"HANDLE : ", m_sock, L"seqID :", m_SeqID.SeqNumberAndIdx, L"seqIndx : ", m_SeqID.idx,
                                       L"IO_Count", m_ioCount);
    }
    CMessage *msg;
    while (m_sendBuffer.Pop(msg))
    {
        delete msg;
    }
    
    m_recvBuffer.ClearBuffer();
}
