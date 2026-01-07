#pragma once

#pragma once
#include "CNetworkLib/CNetworkLib.h"

#include <set>
#include <stack>
#include <unordered_map>
#include <thread>
#include <shared_mutex>
#include "Contents/Sector/SectorManager.h"
#include "Contents/Player/stPlayer.h"
//
//#define dfRANGE_MOVE_TOP 0
//#define dfRANGE_MOVE_LEFT 0
//#define dfRANGE_MOVE_RIGHT 6400
//#define dfRANGE_MOVE_BOTTOM 6400
//// #define dfRANGE_MOVE_RIGHT 640
//// #define dfRANGE_MOVE_BOTTOM 640
//#define dfSECTOR_Size 128
//

constexpr int Chat_Sector_Bottom = 6400;
constexpr int Chat_Sector_Right = 6400;
constexpr int Chat_Sector_Size = 128;

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
  private:
    void MonitorThread();
  public:
    // Stub함수
    virtual void REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *ID, WCHAR *Nickname, WCHAR *SessionKey, WORD wType = en_PACKET_CS_CHAT_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
    virtual void REQ_SECTOR_MOVE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD SectorX, WORD SectorY, WORD wType = en_PACKET_CS_CHAT_REQ_SECTOR_MOVE, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
    virtual void REQ_MESSAGE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD MessageLen, WCHAR *MessageBuffer, WORD wType = en_PACKET_CS_CHAT_REQ_MESSAGE, BYTE bBroadCast = true, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
    virtual void HEARTBEAT(ull SessionID, CMessage *msg, WORD wType = en_PACKET_CS_CHAT__HEARTBEAT, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;

  public:
    void AllocPlayer(CMessage *msg);
    void DeletePlayer(CMessage *msg);

  public:
    CTestServer(int ContentsThreadCnt = 1, int iEncording = false);
    virtual ~CTestServer();



    void Update();
    void BalanceThread();
    void ContentsThread();
    void BalanceUpdate();
    void HeartBeat();

    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    virtual void OnRecv(ull SessionID, CMessage *msg) override;

    virtual bool OnAccept(ull SessionID,SOCKADDR_IN& addr) override;
    virtual void OnRelease(ull SessionID) override;

    virtual LONG64 GetPlayerCount() { return m_TotalPlayers; }

    // Debuging 정보
    ////////////////////////////////////////////////////////////////////////

    LONG64 m_UpdateMessage_Queue; // Update 이후 남아있는 Msg의 수.
    LONG64 m_UpdateTPS;
    LONG64 m_RecvTPS; // OnRecv를 통한 RecvTPS 측정

    LONG64 m_RecvMsgArr[en_PACKET_CS_CHAT__Max]{
        0,
    }; // Update에서 ContentsQ에서 빼는 MsgTPS
    // LONG64 *m_SendMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; //

    LONG64 prePlayer_hash_size = 0;
    LONG64 AccountNo_hash_size = 0;
    LONG64 SessionID_hash_size = 0;

    LONG64 GetprePlayer_hash() const { return prePlayer_hash_size; }
    LONG64 GetAccountNo_hash() const { return AccountNo_hash_size; }
    LONG64 GetSessionID_hash() const { return SessionID_hash_size; } 

    ///////////////////////////////////////////////////////////////////////
    ////////////////////////// ContentsThread //////////////////////////
    // 
    //ContentsThread 만큼 reSize함.
    std::vector<WinThread> hContentsThread_vec; // HANDLE of ContentThread 
    int m_ContentsThreadCnt;  // 생성자를 통해 받은 ContentsQ 개수 .
    
    // ContentsThread가 생성시에 Interlock으로 1씩 증가.
    ull m_ContentsThreadIdX = 0;  

    // Event호출 방식.  [0] 은 초기 Player [ delete, Create ] 
    std::vector<CLockFreeQueue<CMessage*>> m_CotentsQ_vec; // ContentsQ vec
    std::map<CLockFreeQueue<CMessage*> *, HANDLE> m_ContentsQMap; // HANDLE 은 OnRecv후 호출하는 Event

    ////////////////////////////////////////////////////////////////////
    ////////////////////////// BalanceThread //////////////////////////
    //
    WinThread pBalanceThread;
    //  [0] = 기본 큐, 밸런스 대상아님. value는 Player 수 
    std::vector<DWORD> balanceVec;

    HANDLE hBalanceThread; 
    
    WinThread hMonitorThread;

    bool bMonitorThreadOn = true;
    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;

    int m_maxSessions = 0;
    int m_maxPlayers = 20000;

    LONG64 m_prePlayerCount = 0;
    LONG64 m_TotalPlayers = 0; // 현재 Player의 Cnt
    int m_State_Session = 0;   // 아직 Player가 되지못한 Session의 Cnt

    /*

        Player에서 SessionID를 들고있는 현상을 고치기 위해서는.
        ChatingServer 를 기준으로 CPlayer를 갖고 남의 Player에 접근할 일이 없다.

        Sector 자체의 템플릿을 SessionID로 둠으로써 SendPacket에 빠른 접근이 가능하도록하자.
        inline static std::set<ull>
    */
    // TODO: 특정 인원이상 안늘어나게 조치.
    CObjectPool_UnSafeMT<stPlayer> player_pool;

    //SRWLOCK srw_SessionID_Hash; // SessionID_hash 소유권. OnRecv , BalanceThread에서 접근
    SharedMutex srw_SessionID_Hash;
    // Account   Key , Player접근.
    std::unordered_map<ull, stPlayer *> AccountNo_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, stPlayer *> SessionID_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, stPlayer *> prePlayer_hash; // 중복 접속을 제거하는 용도

    // SessionID 를 삽입
     std::set<ull>
        g_Sector[Chat_Sector_Bottom / Chat_Sector_Size]
                [Chat_Sector_Right / Chat_Sector_Size]; // [Y][X]

    stSectorManager m_sectorManager =
        stSectorManager(Chat_Sector_Right, Chat_Sector_Bottom, Chat_Sector_Size);

    // Sector마다 Lock이 존재.  [Y][X]
    SharedMutex
        srw_Sectors[Chat_Sector_Bottom / Chat_Sector_Size][Chat_Sector_Right / Chat_Sector_Size];

    /*
      하트비트 처리 방법.
      일단 LoginPacket을 보내지않는 Session에 대해서 처리 방법이 필요함.

      OnAccept에서 PQCS를하고 그 메세지로 인증되지않은 Player를 생성.
      ContentsThread는 LoginPacket을 받는다면 인증되지않는 Player를 SessionID로 탐색 후 제거에 성공한다면, Player로 승격,

      Contetns는 인증되지않는 Player를 주기적으로 검사한다.

    */

    char RedisIpAddress[IP_LEN];//port 6379

};

