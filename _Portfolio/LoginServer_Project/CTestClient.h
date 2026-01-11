#pragma once
#include "CLanClient/CLanClient.h"

class CTestClient : public CLanClient
{
  public:
    CTestClient(bool bEncoding = false);

    void TimerThread();

  public:
    virtual void OnEnterJoinServer() override ; //	< 서버와의 연결 성공 후
    virtual void OnLeaveServer() override;     //< 서버와의 연결이 끊어졌을 때

    virtual void OnRecv(CClientMessage *msg) override; //< 하나의 패킷 수신 완료 후


    virtual void REQ_MONITOR_LOGIN(CClientMessage *msg, int ServerNo, WORD wType = en_PACKET_SS_MONITOR_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    virtual void REQ_MONITOR_UPDATE(CClientMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType = en_PACKET_SS_MONITOR_DATA_UPDATE, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

    WinThread _hTimer;

};
