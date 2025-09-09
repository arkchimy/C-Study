#pragma once
#include "../../lib/CNetwork_lib/CNetwork_lib.h"

#pragma once

class CSendTestServer : public CLanServer
{
  public:
    CSendTestServer();
    virtual ~CSendTestServer();
    virtual double OnRecv(ull SessionID, CMessage *msg) override;
    virtual bool OnAccept(ull SessionID, SOCKADDR_IN addr) override;

    virtual void RecvPostMessage(clsSession *session) override;

    void EchoProcedure(ull sessionID, CMessage *message);

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool에서 할당하지않음
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread;
};