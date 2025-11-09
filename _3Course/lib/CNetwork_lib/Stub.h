#pragma once
#include "CommonProtocol.h"

using ull = unsigned long long;
class Stub
{
  public:

    virtual bool EchoProcedure(ull SessionID, const char *const buffer) {};                                      // 동적 바인딩
    virtual bool LoginProcedure(ull SessionID, INT64 AccontNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey) {}; // 동적 바인딩
    bool PacketProc(ull SessionID, struct CMessage *msg, class stHeader &header);
};