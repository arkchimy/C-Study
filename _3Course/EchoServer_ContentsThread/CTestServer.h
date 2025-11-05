#pragma once

class CTestServer : public CLanServer
{
  public:
    bool PacketProc(ull SessionID, CMessage *msg);
    void EchoProcedure(ull sessionID, CMessage *message);
  public:
    CTestServer(int iEncording = false);
    virtual ~CTestServer();


    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);

  
    virtual float OnRecv(ull SessionID, CMessage *msg) override;
    virtual bool OnAccept(ull SessionID) override;


    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool에서 할당하지않음
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread = 0;
    HANDLE hMonitorThread = 0 ;

    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;
    int m_maxSessions = 0;
};