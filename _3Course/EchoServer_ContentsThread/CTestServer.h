#pragma once

class CTestServer : public CLanServer
{
  public:
    CTestServer();
    virtual ~CTestServer();
    virtual double OnRecv(ull SessionID, CMessage *msg) override;
    virtual void SendPostMessage(ull SessionID) override; 
    virtual void RecvPostMessage(clsSession* session) override;

    void EchoProcedure(ull sessionID, CMessage * message);

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(CRingBuffer::s_BufferSize,1);

    HANDLE hContentsThread;
};