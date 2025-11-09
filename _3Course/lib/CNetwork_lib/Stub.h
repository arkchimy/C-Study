#pragma once

#include "CommonProtocol.h"

using ull = unsigned long long;
struct CMessage;

class Stub
{
  public:

    virtual bool EchoProcedure(ull SessionID, CMessage *msg, const char *const buffer) { return false; }                       // 동적 바인딩
    virtual bool LoginProcedure(ull SessionID, CMessage *msg, INT64 AccontNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey) { return false; }; // 동적 바인딩
    bool PacketProc(ull SessionID, struct CMessage *msg, class stHeader &header);
};