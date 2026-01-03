#pragma once
#include "./CNetworkLib/CNetworkLib.h"
#include <thread>

struct stPlayer
{

};
class CTestServer : public CLanServer
{

    virtual bool OnAccept(ull SessionID, SOCKADDR_IN &addr) override ;
    virtual void OnRecv(ull SessionID, struct CMessage *msg, bool bBalanceQ = false) override;
    virtual void OnRelease(ull SessionID) override;

    
};
