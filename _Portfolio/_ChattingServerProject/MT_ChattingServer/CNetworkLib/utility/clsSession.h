#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <list>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include "../utility/MT_CRingBuffer/MT_CRingBuffer.h"
#include "../utility//CTlsLockFreeQueue/CTlsLockFreeQueue.h"

using ull = unsigned long long;

enum class Job_Type : BYTE
{
    Recv,
    Send,
    ReleasePost,
    Post,
    MAX,
};
// _mode 판단을 stOverlapped 기준으로 하므로 첫 멤버변수 _mode 로 할것. 
struct stOverlapped : public OVERLAPPED
{
    stOverlapped(Job_Type mode) : _mode(mode) {}
    Job_Type _mode = Job_Type::MAX;
};

struct stSendOverlapped : public OVERLAPPED
{
    stSendOverlapped(Job_Type mode) : _mode(mode) {}
    Job_Type _mode = Job_Type::MAX;
    DWORD msgCnt = 0;
    struct CMessage *msgs[500]{0,};

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
            ull seqNumber : 47;
            ull idx : 17;
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
    stSendOverlapped m_sendOverlapped = stSendOverlapped(Job_Type::Send);
    stSendOverlapped m_releaseOverlapped = stSendOverlapped(Job_Type::ReleasePost);

    CTlsLockFreeQueue<struct CMessage *> m_sendBuffer;
    CRingBuffer m_recvBuffer; 

    stSessionId m_SeqID{0};
    ull m_ioCount = 0;
    ull m_blive = 0;
    ull m_flag = 0; // SendFlag

};
