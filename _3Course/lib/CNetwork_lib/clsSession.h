#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include "../../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
using ull = unsigned long long;

enum class Job_Type
{
    Recv,
    Send,
    PostSend,
    MAX,
};
struct stOverlapped : public OVERLAPPED
{
    stOverlapped(Job_Type mode) : _mode(mode) {}
    Job_Type _mode = Job_Type::MAX;
};

class clsSession
{
  public:
    clsSession(SOCKET sock);
    ~clsSession();

    void Release();

    SOCKET _sock = 0;

    stOverlapped _recvOverlapped = stOverlapped(Job_Type::Recv);
    stOverlapped _sendOverlapped = stOverlapped(Job_Type::Send);
    stOverlapped _PostOverlapped = stOverlapped(Job_Type::PostSend);

    CRingBuffer *sendBuffer; // Echo에서는 미 사용
    CRingBuffer *recvBuffer;

    ull _ioCount = 0;
    ull _blive = 0;
    ull _flag = 0;
};