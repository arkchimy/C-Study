#pragma once
#include "./CZoneNetworkLib/CZoneNetworkLib.h"

#include "clsLoginZone.h"
#include "clsEchoZone.h"

class CTestServer : public CZoneServer
{
    friend class Stub;
    friend class Proxy;

  public:
    void MonitorThread();

    CTestServer(int iEncording)
        : CZoneServer(iEncording)
    {
        // LoginZone은 하나만 등록해야하고.
        RegisterLoginZone<clsLoginZone>(L"LoginServer", 40, (ZoneKeyType)enZoneType::LoginZone);

        // 동일한 방식으로 상속받아 구현한 class를 여기에 등록한다.
        RegisterZone<clsEchoZone>(L"Echo", 20, (ZoneKeyType)enZoneType::EchoZone);

        _MonitorThread = WinThread(&CTestServer::MonitorThread, this);
        
    }
    WinThread _MonitorThread;
};
