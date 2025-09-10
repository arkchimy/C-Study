#pragma once

class CTestServer : public CLanServer
{
  public:
    CTestServer();
    virtual ~CTestServer();
    virtual double OnRecv(ull SessionID, CMessage *msg) override;
    virtual bool OnAccept(ull SessionID, SOCKADDR_IN addr) override;

    virtual void RecvPostMessage(clsSession* session) override;

    void EchoProcedure(ull sessionID, CMessage * message);

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool에서 할당하지않음 
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread;
    HANDLE hMonitorThread;
    LONG64 m_TotalTPS = 0;
};