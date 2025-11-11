#pragma once
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include <stack>
#include <unordered_map>

struct CPlayer
{
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

    void DeletePlayer(CMessage* msg);

  public:
    CTestServer(int iEncording = false);
    virtual ~CTestServer();


    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);

  
    virtual float OnRecv(ull SessionID, CMessage *msg) override;
    virtual bool OnAccept(ull SessionID) override;
    virtual LONG64 GetPlayerCount() { return m_TotalPlayers; }

    SRWLOCK srw_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool에서 할당하지않음
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread = 0;
    HANDLE hMonitorThread = 0 ;

    // TODO: 특정 인원이상 안늘어나게 조치.
    CObjectPool_UnSafeMT<CPlayer> player_pool; 

    // Account   Key , Player접근.
    std::unordered_map<ull, CPlayer*> AccountNo_hash; // 중복 접속을 제거하는 용도
    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer*> SessionID_hash; // 중복 접속을 제거하는 용도


    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;
    int m_maxSessions = 0;   
    int m_maxPlayers = 7000;   
    LONG64 m_TotalPlayers = 0; // 현재 Player의 Cnt
    int m_State_Session = 0; // 아직 Player가 되지못한 Session의 Cnt
};