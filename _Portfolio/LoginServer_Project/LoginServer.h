#pragma once
#include "CNetworkLib/CNetworkLib.h"
#include "CrushDump_lib/CrushDump_lib.h"

#include <set>
#include <stack>
#include <unordered_map>
#include <thread>
#include <shared_mutex>

enum class en_State : int
{
    Session,
    Player,
    DisConnect,
    Max,
};

struct stDBOverapped : public OVERLAPPED
{
    ull SessionID;
    CMessage *msg;
};

struct CPlayer
{
    DWORD m_ContentsQIdx = 0;
    en_State m_State = en_State::Max;
    ull m_sessionID = 0;

    DWORD m_Timer = 0;
    INT64 m_AccountNo = 0;

    WCHAR m_ID[20]{0};
    WCHAR m_Nickname[20]{0};
    char m_SessionKey[64]{0};
    stDBOverapped DB_ReqOverlapped;
};


class CTestServer :public CLanServer
{
    virtual void REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType = en_PACKET_CS_LOGIN_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0);

     public:
    void AllocPlayer(CMessage *msg);
    void DeletePlayer(CMessage *msg);
    void Update();
  public:
    CTestServer(DWORD ContentsThreadCnt = 1, int iEncording = false, int reduceThreadCount = 0);
    virtual ~CTestServer();

    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    virtual float OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ = false) override;

    virtual bool OnAccept(ull SessionID) override;
    virtual void OnRelease(ull SessionID) override;


    std::queue<CMessage *> JobQueue;
    std::shared_mutex JobQueue_Lock;


    ////////////////////////// ContentsThread //////////////////////////
    HANDLE m_hDBIOCP = INVALID_HANDLE_VALUE;

    DWORD m_ContentsThreadCnt;

    std::vector<std::thread> hContentsThread_vec;
    inline static ringBufferSize s_ContentsQsize;

    HANDLE hMonitorThread = 0;
    bool bMonitorThreadOn = true;
    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;

    int m_maxSessions = 0;
    int m_maxPlayers = 20000;

    LONG64 m_prePlayerCount = 0;
    LONG64 m_TotalPlayers = 0; // 현재 Player의 Cnt
    int m_State_Session = 0;   // 아직 Player가 되지못한 Session의 Cnt

    int m_maxSessions = 0;
    int m_maxPlayers = 20000;

    LONG64 m_prePlayerCount = 0;
    LONG64 m_TotalPlayers = 0; // 현재 Player의 Cnt
    int m_State_Session = 0;   // 아직 Player가 되지못한 Session의 Cnt

    LONG64 m_DBMessageCnt;
    /*

        Player에서 SessionID를 들고있는 현상을 고치기 위해서는.
        ChatingServer 를 기준으로 CPlayer를 갖고 남의 Player에 접근할 일이 없다.

        Sector 자체의 템플릿을 SessionID로 둠으로써 SendPacket에 빠른 접근이 가능하도록하자.
    */

    // TODO: 특정 인원이상 안늘어나게 조치.
    CObjectPool_UnSafeMT<CPlayer> player_pool;
    std::shared_mutex SessionID_hash_Lock;

    // Account   Key , Player접근.
    std::unordered_map<ull, CPlayer *> AccountNo_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> SessionID_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> prePlayer_hash; // 중복 접속을 제거하는 용도

};
//// Debuging 정보
//////////////////////////////////////////////////////////////////////////
//
//LONG64 m_UpdateMessage_Queue; // Update 이후 남아있는 Msg의 수.
//LONG64 m_UpdateTPS;
//LONG64 m_RecvTPS; // OnRecv를 통한 RecvTPS 측정
//
//LONG64 *m_RecvMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; // Update에서 ContentsQ에서 빼는 MsgTPS
//// LONG64 *m_SendMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; //
//
//LONG64 prePlayer_hash_size = 0;
//LONG64 AccountNo_hash_size = 0;
//LONG64 SessionID_hash_size = 0;
//
//     virtual LONG64 GetPlayerCount() { return m_TotalPlayers; }
//LONG64 GetprePlayer_hash() { return prePlayer_hash_size; }
//LONG64 GetAccountNo_hash() { return AccountNo_hash_size; }
//LONG64 GetSessionID_hash() { return SessionID_hash_size; }
//
/////////////////////////////////////////////////////////////////////////