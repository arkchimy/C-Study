#include "clsSession.h"
#include "../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"


clsSession::clsSession(SOCKET sock)
    : _sock(sock)
{

    sendBuffer = new CRingBuffer();
    recvBuffer = new CRingBuffer();
    _blive = 1;

}

clsSession::~clsSession()
{
    closesocket(_sock);
}

void clsSession::Release()
{
    _blive = 0;
}
