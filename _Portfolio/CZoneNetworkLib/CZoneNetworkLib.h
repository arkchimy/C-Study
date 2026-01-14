#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
#include "CNetworkLib/CNetworkLib.h"

class CZoneServer : public CLanServer
{
  protected:
    CZoneServer(int iEncoding, IZone* LoginZone)
        : CLanServer(iEncoding)
    {
        RegisterLoginZone(LoginZone, INFINITE);
    }

    virtual ~CZoneServer()
    {
        // TODO : _zoneList 를 돌며 할당해제.
    }
  private:
      // 반드시 재정의
    virtual bool OnAccept(ull SessionID, SOCKADDR_IN &addr);
    virtual void OnRecv(ull SessionID, struct CMessage *msg);
    virtual void OnRelease(ull SessionID) ;

  public:
    void RegisterLoginZone(IZone *zone, int deltaTime );
    void RegisterZone(IZone *zone, const wchar_t *ThreadName, int deltaTime);

    //////////////////////////////////////////
    private:
    ZoneSet *m_LoginZone = nullptr;
    std::list<ZoneSet *> _zoneList;
    HANDLE _hLoginEvent = INVALID_HANDLE_VALUE;

    ull _bLoginZoneChk = false;

};