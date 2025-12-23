#pragma once
#include "CNetworkLib/CNetworkLib.h"

class LobbyZoen : public IZone
{
  protected:
    virtual void OnAccept(ull SessionId, SOCKADDR_IN &addr) ;
    virtual void OnRecv(ull SessionId, struct CMessage *msg);
    virtual void OnUpdate() ;
    virtual void OnRelease(ull SessiondId) ;
};

