#include "clsSession.h"


clsSession::clsSession(SOCKET sock)
    : _sock(sock)
{
    _sendOverlapped._mode = Job_Type::Send;
    _recvOverlapped._mode = Job_Type::Recv;
    sendBuffer = new CRingBuffer(10000);
    recvBuffer = new CRingBuffer();
    blive = 1;
    InitializeSRWLock(&srw_session);

}

clsSession::~clsSession()
{
    
}
