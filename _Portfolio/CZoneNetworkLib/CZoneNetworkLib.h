#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
#include "CNetworkLib/CNetworkLib.h"

enum enZoneMsgType : uint8_t
{
    EnterZone,
    LeaveZone,
};
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
      //  사실  session을 직접받는것이 성능이 좋으나
      // networklib를 최소한으로 건드려서 그대로 복붙희망.
    virtual bool OnAccept(ull SessionID, SOCKADDR_IN &addr) final;
    virtual void OnRecv(ull SessionID, struct CMessage *msg) final;
    virtual void OnRelease(ull SessionID) final ;

  public:
    void RegisterLoginZone(IZone *zone, int deltaTime );
    void RegisterZone(IZone *zone, const wchar_t *ThreadName, int deltaTime);

    void RequeseMoveZone(ull SessionID, IZone *targetZone);
    //////////////////////////////////////////
    private:
    ZoneSet *m_LoginZone = nullptr;
    std::list<ZoneSet *> _zoneList;

    std::map<IZone *, ZoneSet *> _zoneMap;

    HANDLE _hLoginEvent = INVALID_HANDLE_VALUE;

    ull _bLoginZoneChk = false;

};