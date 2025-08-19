#pragma once

class CTestServer : public CLanServer
{
  public:
    CTestServer();
    virtual ~CTestServer();
    virtual double OnRecv(ull SessionID, CMessage *msg) override;

    CRITICAL_SECTION cs_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(100000);

    HANDLE hContentsThread;
};