#pragma once
#include "stHeader.h"
#include "CommonProtocol.h"
#include "ServerProtocol.h"

using ull = unsigned long long;
struct CMessage;
struct stHeader;

class Stub
{
  public:

    virtual void EchoProcedure(ull SessionID, CMessage *msg, const char *const buffer, short len) { }                            // 동적 바인딩
    virtual void LoginProcedure(ull SessionID, CMessage *msg, INT64 AccontNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey) { }; // 동적 바인딩
    void PacketProc(ull SessionID,  CMessage *msg, stHeader &header, WORD type);
};