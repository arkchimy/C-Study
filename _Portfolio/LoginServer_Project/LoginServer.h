#pragma once
#include "CNetworkLib/CNetworkLib.h"
#include "CrushDump_lib/CrushDump_lib.h"

#include <set>
#include <shared_mutex>
#include <stack>
#include <thread>
#include <unordered_map>

enum class en_State : ull
{
    None,
    LoginWait,
    Max,
};

struct CPlayer
{
    en_State m_State = en_State::None;
    ull m_sessionID = 0;

    DWORD m_Timer = 0;
    INT64 m_AccountNo = 0;

    WCHAR m_ID[20]{0};
    WCHAR m_Nickname[20]{0};
    char m_SessionKey[64]{0};

    std::string m_ipAddress;
    USHORT m_port;
};

// DB연동서버
enum
{
    IP_LEN = 16,
    DBName_LEN = 16,
    schema_LEN = 16,
    ID_LEN = 16,
    Password_LEN = 16,
};

class CTestServer : public CLanServer
{
  public:
    virtual void REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType = en_PACKET_CS_LOGIN_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0);

  public:
    // void AllocPlayer(CMessage *msg);
    // void DeletePlayer(CMessage *msg);
    //////////////////////////////////////////////////////// 하트 비트 전용////////////////////////////////////////////////////////
    void Update();
    //////////////////////////////////////////////////////// DB에서 토큰 대조하는 함수 ////////////////////////////////////////////////////////
    void DB_VerifySession(CMessage *msg);
    BYTE WaitDB(INT64 AccountNo, const WCHAR *const SessionKey, WCHAR *ID, WCHAR *Nick);

  private:
    void MonitorThread();
    void HeartBeatThread();
    void DBworkerThread();

  public:
    CTestServer(DWORD ContentsThreadCnt = 1, int iEncording = false, int reduceThreadCount = 0);
    virtual ~CTestServer();

    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    virtual void OnRecv(ull SessionID, CMessage *msg) override;

    virtual bool OnAccept(ull SessionID, SOCKADDR_IN &addr) override;
    virtual void OnRelease(ull SessionID) override;

    WinThread hMonitorThread;
    bool bMonitorThreadOn = true;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;

  private:
    WinThread hHeartBeatThread;

    ////////////////////////// HeartBeatThread  //////////////////////////

    ////////////////////////// jobQueue of DBThread  //////////////////////////
    HANDLE m_hDBIOCP = INVALID_HANDLE_VALUE;
    std::queue<CMessage *> JobQueue;
    SharedMutex JobQueue_Lock;

    DWORD m_ContentsThreadCnt;

    std::vector<WinThread> hDBThread_vec;
    // inline static ringBufferSize s_ContentsQsize;

    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;

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
    CObjectPool<stDBOverlapped> dbOverlapped_pool;
    SharedMutex SessionID_hash_Lock;

    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> SessionID_hash; // 중복 접속을 제거하는 용도
    std::unordered_map<ull, CPlayer *> Account_hash;   // 중복 접속을 제거하는 용도

  public:
    WCHAR GameServerIP[16] = L"0.0.0.0";
    USHORT GameServerPort = 0;

    WCHAR ChatServerIP[16]{};
    WCHAR Dummy1_ChatServerIP[16]{};
    WCHAR Dummy2_ChatServerIP[16]{};
    USHORT ChatServerPort = 6000;

    // DB연동서버

    char AccountDB_IPAddress[IP_LEN];
    char RedisIpAddress[IP_LEN];
    USHORT DBPort;
    char DBName[DBName_LEN];
    char schema[schema_LEN];

    char DBuser[ID_LEN];
    char password[Password_LEN];
    int m_lProcessCnt;
    LONG64 m_UpdateTPS = 0; // DB처리 TPS
};
//// Debuging 정보
//////////////////////////////////////////////////////////////////////////
//
// LONG64 m_UpdateMessage_Queue; // Update 이후 남아있는 Msg의 수.
// LONG64 m_UpdateTPS;
// LONG64 m_RecvTPS; // OnRecv를 통한 RecvTPS 측정
//
// LONG64 *m_RecvMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; // Update에서 ContentsQ에서 빼는 MsgTPS
//// LONG64 *m_SendMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; //
//
// LONG64 prePlayer_hash_size = 0;
// LONG64 AccountNo_hash_size = 0;
// LONG64 SessionID_hash_size = 0;
//
//     virtual LONG64 GetPlayerCount() { return m_TotalPlayers; }
// LONG64 GetprePlayer_hash() { return prePlayer_hash_size; }
// LONG64 GetAccountNo_hash() { return AccountNo_hash_size; }
// LONG64 GetSessionID_hash() { return SessionID_hash_size; }
//
/////////////////////////////////////////////////////////////////////////