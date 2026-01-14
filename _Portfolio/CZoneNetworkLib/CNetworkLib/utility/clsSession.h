#pragma once
#define WIN32_LEAN_AND_MEAN
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <list>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include "../utility/MT_CRingBuffer/MT_CRingBuffer.h"
#include "../utility//CTlsLockFreeQueue/CTlsLockFreeQueue.h"
#include "../utility/CLockFreeQueue/CLockFreeQueue.h"
#include "../WinAPI/WinThread.h"


using ull = unsigned long long;
class clsSession;

enum class Job_Type : BYTE
{
    Recv,
    Send,
    // 해당 msg의 완료통지로 세션 끊김 절차.
    ReleasePost,
    Post,
    MAX,
};
// _mode 판단을 stOverlapped 기준으로 하므로 첫 멤버변수 _mode 로 할것. 
struct stOverlapped : OVERLAPPED
{
    Job_Type _mode;
    stOverlapped(Job_Type m) : _mode(m) {}
};

struct stSendOverlapped : stOverlapped
{
    DWORD msgCnt = 0;
    CMessage *msgs[500]{};
    stSendOverlapped() : stOverlapped(Job_Type::Send) {}
};

struct stPostOverlapped : stOverlapped
{
    CMessage *msg = nullptr;
    stPostOverlapped() : stOverlapped(Job_Type::Post) {}
};

struct stReleaseOverlapped : stOverlapped
{
    stReleaseOverlapped() : stOverlapped(Job_Type::ReleasePost) {}
};



class IZone
{
  public:
    virtual void OnAccept(ull SessionId, SOCKADDR_IN &addr) = 0;
    virtual void OnRecv(ull SessionId, struct CMessage *msg) = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRelease(ull SessiondId) = 0;
};

class ZoneSet
{
  public:
    ZoneSet(IZone *zone, const wchar_t *ThreadName, bool *bOn, int deltaTime , HANDLE hEvent = INVALID_HANDLE_VALUE);

    ~ZoneSet()
    {
        m_Thread.join();
    }
    void Push(CMessage *msg)
    {
        q.Push(msg);
    }

  private:
    void ThreadMain();
    void RegisterEventHandle(HANDLE hEvent);
  private:
    IZone *m_zone;
    WinThread m_Thread;
    int _deltaTime;

    CLockFreeQueue<CMessage *> q;
    //해당 Zone에 존재하는 session들.
    std::list<clsSession *> sessions;

  public:
    bool *m_bOn; // Server의 Running을 가리키는 포인터
    HANDLE _hEvent;
};




class clsSession
{
  public:
    clsSession() = default;
    clsSession(SOCKET sock);
    ~clsSession();

    void Release();

    SOCKET m_sock = 0;
    stOverlapped m_recvOverlapped = stOverlapped(Job_Type::Recv);

    stSendOverlapped m_sendOverlapped;
    stReleaseOverlapped m_releaseOverlapped;

    CTlsLockFreeQueue<CMessage *> m_sendBuffer;
    CTlsLockFreeQueue<struct CMessage *> m_ZoneBuffer;

    CRingBuffer m_recvBuffer; 

    ull m_SeqID = 0;
    ull m_ioCount = 0;
    ull m_blive = 0;
    ull m_flag = 0; // SendFlag

    ZoneSet *m_zoneSet;

};
