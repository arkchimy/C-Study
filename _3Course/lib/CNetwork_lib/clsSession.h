#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <list>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include "../../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
using ull = unsigned long long;

enum class Job_Type
{
    Recv,
    Send,
    MAX,
};
struct stOverlapped : public OVERLAPPED
{
    stOverlapped(Job_Type mode) : _mode(mode) {}
    Job_Type _mode = Job_Type::MAX;
};
typedef struct stSessionId
{
    bool operator==(const stSessionId other)
    {
        return SeqNumberAndIdx == other.SeqNumberAndIdx;
    }

    union
    {
        struct
        {
            ull idx : 16;
            ull seqNumber : 48;
        };
        ull SeqNumberAndIdx = 0;
    };

} stSessionId;

class clsSession
{
  public:
    clsSession() = default;
    clsSession(SOCKET sock);
    ~clsSession();

    void Release();

    SOCKET m_sock = 0;

    stOverlapped m_recvOverlapped = stOverlapped(Job_Type::Recv);
    stOverlapped m_sendOverlapped = stOverlapped(Job_Type::Send);

    CRingBuffer m_sendBuffer; // Echo에서는 미 사용
    CRingBuffer m_recvBuffer; 

    WSABUF m_lastRecvWSABuf[2]{0};


    std::list<struct CMessage *> m_SendMsg;

    stSessionId m_SeqID{0};
    ull m_ioCount = 0;
    ull m_blive = 0;
    ull m_flag = 0;
    ull m_Useflag = 0; // RelaseFlag 임


};
