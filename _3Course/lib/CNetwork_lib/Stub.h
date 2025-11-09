#pragma once
#include "stHeader.h"
#include "CommonProtocol.h"

using ull = unsigned long long;
struct CMessage;
struct stHeader;

class Stub
{
  public:

    virtual bool EchoProcedure(ull SessionID, CMessage *msg, const char *const buffer,short len) { return false; }                       // 동적 바인딩
    virtual bool LoginProcedure(ull SessionID, CMessage *msg, INT64 AccontNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey) { return false; }; // 동적 바인딩
    bool PacketProc(ull SessionID,  CMessage *msg, stHeader &header);
};