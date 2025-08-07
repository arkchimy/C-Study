#pragma once
#include "../lib/MT_CRingBuffer_lib/framework.h"
#include <Windows.h>
#include <WinSock2.h>

using ull = unsigned long long;

enum class Job_Type
{
    Recv,
    Send,
    MAX,
};
struct stOverlapped : public OVERLAPPED
{
    Job_Type _mode = Job_Type::MAX;
};

class clsSession
{
  public:
    clsSession(SOCKET sock);
    ~clsSession();

    SOCKET _sock = 0;
    stOverlapped _sendOverlapped;
    stOverlapped _recvOverlapped;

    CRingBuffer *sendBuffer;
    CRingBuffer *recvBuffer;

    SRWLOCK srw_session;

    ull _ioCount = 0;
    ull blive = 0;
    ull flag = 0;
};