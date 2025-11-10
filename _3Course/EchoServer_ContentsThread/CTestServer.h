#pragma once
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include <stack>
#include <unordered_map>

struct CPlayer
{
    en_State m_State = en_State::DisConnect;
    ull m_sessionID;

    DWORD m_Timer;
    INT64 m_AccountNo;
    WCHAR m_ID[20];
    WCHAR m_Nickname[20];
    char m_SessionKey[64];

    int iSectorX = 0;
    int iSectorY = 0;
};

class CTestServer : public CLanServer
{
  public:
    
    virtual void EchoProcedure(ull sessionID, CMessage *msg, char *const const buffer, short len) final;
    virtual void LoginProcedure(ull SessionID, CMessage *msg, INT64 AccontNo, WCHAR *ID, WCHAR *Nickname, char *SessionKey) final; // 동적 바인딩

    void InitalizePlayer(CMessage* msg);
  public:
    CTestServer(int iEncording = false);
    virtual ~CTestServer();


    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);

  
    virtual float OnRecv(ull SessionID, CMessage *msg) override;
    virtual bool OnAccept(ull SessionID) override;

    bool GetId(short& idx);

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool에서 할당하지않음
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread = 0;
    HANDLE hMonitorThread = 0 ;

    // Player의 자료구조를 어떤 것으로 둘까?
    CObjectPool_UnSafeMT<CPlayer> player_pool;

    std::unordered_map<ull, CPlayer*> player_hash;
    //std::vector<class CPlayer> player_vec; //
    std::stack<short> idle_stack;

    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;
    int m_maxSessions = 0;
};