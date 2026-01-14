#pragma once
#include <cstdint>
#include "../CNetworkLib/CNetworkLib.h"
#include "../CNetworkLib/WinAPI/WinThread.h"


using ull = uint64_t;
struct CMessage;
class clsSession;

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
    ZoneSet(IZone *zone, const wchar_t *ThreadName, bool *bOn, int deltaTime);

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

  private:
    IZone *m_zone;
    WinThread m_Thread;
    int _deltaTime;

    CLockFreeQueue<CMessage *> q;
    std::list<clsSession *> sessions;

  public:
    bool *m_bOn; // Server의 Running을 가리키는 포인터
};
