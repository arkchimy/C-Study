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
    // 해당 msg의 완료통지로 세션 끊김 절차.
    ReleasePost,
    Post,
    MAX,
};
// _mode 판단을 stClientOverlapped 기준으로 하므로 첫 멤버변수 _mode 로 할것. 
struct stClientOverlapped : OVERLAPPED
{
    Job_Type _mode;
    stClientOverlapped(Job_Type m) : _mode(m) { }
};

struct stClientSendOverlapped : stClientOverlapped
{
    DWORD msgCnt = 0;
    CClientMessage *msgs[500]{};
    stClientSendOverlapped() : stClientOverlapped(Job_Type::Send) {}
};

struct stClientPostOverlapped : stClientOverlapped
{
    CClientMessage *msg = nullptr;
    stClientPostOverlapped() : stClientOverlapped(Job_Type::Post) {}
};

struct stClientReleaseOverlapped : stClientOverlapped
{
    stClientReleaseOverlapped() : stClientOverlapped(Job_Type::ReleasePost) {}
};

class clsClientSession
{
  public:
    clsClientSession()
    {
        const char ch[] = "CClientMessage";
    }
    clsClientSession(SOCKET sock);
    ~clsClientSession();

    void Release();

    SOCKET m_sock = 0;
    stClientOverlapped m_recvOverlapped = stClientOverlapped(Job_Type::Recv);
    stClientSendOverlapped m_sendOverlapped;
    stClientReleaseOverlapped m_releaseOverlapped;

    CTlsLockFreeQueue<CClientMessage *> m_sendBuffer;
    CRingBuffer m_recvBuffer; 

    ull m_SeqID = 0;
    ull m_ioCount = 0;
    ull m_blive = 0;
    ull m_flag = 0; // SendFlag

};
