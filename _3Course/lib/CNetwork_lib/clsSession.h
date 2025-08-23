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

    SOCKET m_sock = 0;

    stOverlapped m_recvOverlapped = stOverlapped(Job_Type::Recv);
    stOverlapped m_sendOverlapped = stOverlapped(Job_Type::Send);
    stOverlapped m_postOverlapped = stOverlapped(Job_Type::PostSend);

    CRingBuffer *m_sendBuffer; // Echo에서는 미 사용
    CRingBuffer *m_recvBuffer;

    ull m_ioCount = 0;
    ull m_blive = 0;
    ull m_flag = 0;
    ull m_Postflag = 0;
    ull m_id = 0;
};