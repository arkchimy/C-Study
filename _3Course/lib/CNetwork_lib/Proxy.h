#pragma once

#include "CommonProtocol.h"

using ull = unsigned long long;
struct CMessage;
class Proxy
{

  public:
    enum En_SendPackType : BYTE
    {
        UnitCast = 0,
        BroadCast = 1,
        SectorAround = 2,

    };
    void LoginProcedure(ull SessionID, CMessage *msg, INT64 AccountNo);
    void EchoProcedure(ull SessionID, CMessage *msg, char *const buffer, short len);
    
};
