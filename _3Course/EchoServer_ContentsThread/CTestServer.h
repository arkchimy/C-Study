#pragma once
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include <stack>
#include <unordered_map>
    #include <set>

#define dfRANGE_MOVE_TOP 0
#define dfRANGE_MOVE_LEFT 0
#define dfRANGE_MOVE_RIGHT 6400
#define dfRANGE_MOVE_BOTTOM 6400
// #define dfRANGE_MOVE_RIGHT 640
// #define dfRANGE_MOVE_BOTTOM 640
#define dfSECTOR_Size 128

enum class en_State : int
{
    Session,
    Player,
    DisConnect,
    Max,
};


struct CPlayer
{
    en_State m_State = en_State::Max;
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

    void AllocPlayer(CMessage* msg);
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


    HANDLE m_ContentsEvent = INVALID_HANDLE_VALUE;
    HANDLE m_ServerOffEvent = INVALID_HANDLE_VALUE;

    int m_maxSessions = 0;   
    int m_maxPlayers = 7000;   

    LONG64 m_prePlayerCount = 0;
    LONG64 m_TotalPlayers = 0; // 현재 Player의 Cnt
    int m_State_Session = 0; // 아직 Player가 되지못한 Session의 Cnt



    /*
        
        Player에서 SessionID를 들고있는 현상을 고치기 위해서는.
        ChatingServer 를 기준으로 CPlayer를 갖고 남의 Player에 접근할 일이 없다.

        Sector 자체의 템플릿을 SessionID로 둠으로써 SendPacket에 빠른 접근이 가능하도록하자.
        inline static std::set<ull>   
    */
    // TODO: 특정 인원이상 안늘어나게 조치.
    CObjectPool_UnSafeMT<CPlayer> player_pool;

    // Account   Key , Player접근.
    std::unordered_map<ull, CPlayer *> AccountNo_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> SessionID_hash; // 중복 접속을 제거하는 용도
    
    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> prePlayer_hash; // 중복 접속을 제거하는 용도


    // SessionID 를 삽입 
    inline static std::set<ull>
        g_Sector[dfRANGE_MOVE_BOTTOM / dfSECTOR_Size]
                [dfRANGE_MOVE_BOTTOM / dfSECTOR_Size];

    /*
      하트비트 처리 방법. 
      일단 LoginPacket을 보내지않는 Session에 대해서 처리 방법이 필요함.
      
      OnAccept에서 PQCS를하고 그 메세지로 인증되지않은 Player를 생성.
      ContentsThread는 LoginPacket을 받는다면 인증되지않는 Player를 SessionID로 탐색 후 제거에 성공한다면, Player로 승격,
      
      Contetns는 인증되지않는 Player를 주기적으로 검사한다.
      
    */

};