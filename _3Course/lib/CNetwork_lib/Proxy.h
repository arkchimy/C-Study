#pragma once

#include "CommonProtocol.h"

using ull = unsigned long long;
struct CMessage;
class Proxy
{

  public:
    enum En_SendPackType : BYTE
    {
        BroadCast = 0,
        UnitCast = 1,
        SectorAround = 2,

    };
    void LoginProcedure(ull SessionID, CMessage *msg, INT64 AccountNo);
    void EchoProcedure(ull SessionID, CMessage *msg, char *const buffer, short len);
    
};
