#pragma once
class Stub
{
  public:
    //virtual void CreatePlayer(Section *clpSection, int id, BYTE byDirection, short x, short y, BYTE byHP, BYTE byType = dfPACKET_SC_CREATE_MY_CHARACTER, BYTE bBroadCast = 0) {}
    virtual void EchoProcedure(ull SessionID, const char *const buffer){};//동적 바인딩
    bool PacketProc(ull SessionID, struct CMessage *msg, class stHeader &header);
};