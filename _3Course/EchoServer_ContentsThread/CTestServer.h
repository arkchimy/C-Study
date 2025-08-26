#pragma once

class CTestServer : public CLanServer
{
  public:
    CTestServer();
    virtual ~CTestServer();
    virtual double OnRecv(ull SessionID, CMessage *msg) override;
    virtual void SendPostMessage(ull SessionID) override; 


    void EchoProcedure(CMessage *const message);

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ;

    HANDLE hContentsThread;
};