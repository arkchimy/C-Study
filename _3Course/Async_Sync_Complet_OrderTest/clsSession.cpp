#include "clsSession.h"
#include "../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"


clsSession::clsSession(SOCKET sock)
    : _sock(sock)
{

    sendBuffer = new CRingBuffer(1000);
    recvBuffer = new CRingBuffer();
    _blive = 1;
    InitializeSRWLock(&srw_session);

}

clsSession::~clsSession()
{
    
}
