#pragma once

class CTestServer : public CLanServer
{
  public:
    CTestServer();
    virtual ~CTestServer();
    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);

    virtual double OnRecv(ull SessionID, CMessage *msg) override;
    virtual bool OnAccept(ull SessionID) override;

    void EchoProcedure(ull sessionID, CMessage *message);

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool���� �Ҵ���������
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread = 0;
    HANDLE hMonitorThread = 0 ;

    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;
};