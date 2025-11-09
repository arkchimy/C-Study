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

    bool LoginProcedure(ull SessionID, CMessage *msg, INT64 AccountNo);
    void ReqEcho(ull SessionID,const char* buffer,DWORD len);
 /*   void CreatePlayer(Section *clpSection, int id, BYTE byDirection, short x,
                      short y, BYTE byHP,
                      BYTE byType = dfPACKET_SC_CREATE_MY_CHARACTER,
                      BYTE bBroadCast = 0);
                      */
    
};
