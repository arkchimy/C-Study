#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <list>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include "../../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
#include "../../lib/CLockFreeQueue_lib/CLockFreeQueue_lib.h"

#include "../../../_4Course/_lib/CTlsLockFreeQueue/CTlsLockFreeQueue.h"

using ull = unsigned long long;

#define SESSION_IDX_MASK 0xFFFF800000000000
#define SESSION_SEQ_MASK 0x00007FFFFFFFFFFF

enum class Job_Type : BYTE
{
    Recv,
    Send,
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
    DWORD m_AcceptTime = 0;

    stOverlapped m_recvOverlapped = stOverlapped(Job_Type::Recv);
    stSendOverlapped m_sendOverlapped = stSendOverlapped(Job_Type::Send);

    CTlsLockFreeQueue<struct CMessage *> m_sendBuffer;
    CRingBuffer m_recvBuffer; 

    stSessionId m_SeqID{0};
    ull m_ioCount = 0;
    ull m_blive = 0;
    ull m_flag = 0; // SendFlag


};
