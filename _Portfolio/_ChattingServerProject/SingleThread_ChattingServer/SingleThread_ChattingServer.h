#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "./CNetworkLib/CNetworkLib.h"
#include "./CNetworkLib/RPC/Proxy.h"

#include <set>
#include <stack>
#include <unordered_map>

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
    void Initalize() 
    {
        if (paddin1 != 0xedededed || paddin2 != 0xedededed)
            __debugbreak();
         m_State = en_State::Max;
         m_sessionID = 0;

         m_Timer = 0;
         m_AccountNo = 0;

        iSectorX = 0;
        iSectorY = 0;
    }
    int paddin1 = 0xedededed;
    en_State m_State = en_State::Max;
    ull m_sessionID = 0;

    DWORD m_Timer = 0;
    INT64 m_AccountNo = 0;

    WCHAR m_ID[20]{0};
    WCHAR m_Nickname[20]{0};
    char m_SessionKey[64]{0};

    int iSectorX = 0;
    int iSectorY = 0;
    int paddin2 = 0xedededed;
};

class CTestServer : public CLanServer
{
  public:
    // Stub함수

    virtual void REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *ID, WCHAR *Nickname, WCHAR *SessionKey, WORD wType = en_PACKET_CS_CHAT_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
    virtual void REQ_SECTOR_MOVE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD SectorX, WORD SectorY, WORD wType = en_PACKET_CS_CHAT_REQ_SECTOR_MOVE, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
    virtual void REQ_MESSAGE(ull SessionID, CMessage *msg, INT64 AccountNo, WORD MessageLen, WCHAR *MessageBuffer, WORD wType = en_PACKET_CS_CHAT_REQ_MESSAGE, BYTE bBroadCast = true, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
    virtual void HEARTBEAT(ull SessionID, CMessage *msg, WORD wType = en_PACKET_CS_CHAT__HEARTBEAT, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, WORD wVectorLen = 0) final;
  public:
    void AllocPlayer(CMessage *msg);
    void DeletePlayer(CMessage *msg);
    void HeartBeat();

  public:
    CTestServer(int iEncording = false);
    virtual ~CTestServer();

    void Update();

    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);

    virtual float OnRecv(ull SessionID, CMessage *msg, bool bBalance = false) override;
    virtual bool OnAccept(ull SessionID) override;
    virtual void OnRelease(ull SessionID) override;

    virtual LONG64 GetPlayerCount() { return m_TotalPlayers; }

    // Debuging 정보
    ////////////////////////////////////////////////////////////////////////

    LONG64 m_UpdateMessage_Queue; // Update 이후 남아있는 Msg의 수.
    LONG64 m_UpdateTPS;
    LONG64 m_RecvTPS; // OnRecv를 통한 RecvTPS 측정

    LONG64 *m_RecvMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; // Update에서 ContentsQ에서 빼는 MsgTPS
    // LONG64 *m_SendMsgArr = new LONG64[en_PACKET_CS_CHAT__Max]; //

    LONG64 prePlayer_hash_size = 0;
    LONG64 AccountNo_hash_size = 0;
    LONG64 SessionID_hash_size = 0;

    LONG64 GetprePlayer_hash() { return prePlayer_hash_size; }
    LONG64 GetAccountNo_hash() { return AccountNo_hash_size; }
    LONG64 GetSessionID_hash() { return SessionID_hash_size; }

    ///////////////////////////////////////////////////////////////////////

    //SRWLOCK srw_ContentQ;

    CLockFreeQueue<CMessage *> m_ContentsQ;
    //CRingBuffer m_ContentsQ = CRingBuffer(s_ContentsQsize, 1); // Pool에서 할당하지않음
    inline static ringBufferSize s_ContentsQsize;
    HANDLE hContentsThread = 0;
    HANDLE hMonitorThread = 0;

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
    CObjectPool_UnSafeMT<CPlayer> player_pool;

    // Account   Key , Player접근.
    std::unordered_map<ull, CPlayer *> AccountNo_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> SessionID_hash; // 중복 접속을 제거하는 용도

    // SessionID Key , Player접근.
    std::unordered_map<ull, CPlayer *> prePlayer_hash; // 중복 접속을 제거하는 용도

    // SessionID 를 삽입
    int paddin1 = 0xedededed;
    std::set<ull>
        g_Sector[dfRANGE_MOVE_BOTTOM / dfSECTOR_Size]
                [dfRANGE_MOVE_BOTTOM / dfSECTOR_Size];
    int paddin2 = 0xedededed;
    /*
      하트비트 처리 방법.
      일단 LoginPacket을 보내지않는 Session에 대해서 처리 방법이 필요함.

      OnAccept에서 PQCS를하고 그 메세지로 인증되지않은 Player를 생성.
      ContentsThread는 LoginPacket을 받는다면 인증되지않는 Player를 SessionID로 탐색 후 제거에 성공한다면, Player로 승격,

      Contetns는 인증되지않는 Player를 주기적으로 검사한다.

    */
};

struct st_Sector_Pos
{
    st_Sector_Pos() = default;

    st_Sector_Pos(int iX, int iY)
    {
        _iX = iX / dfSECTOR_Size;
        _iY = iY / dfSECTOR_Size;
    }
    bool operator<(const st_Sector_Pos &other) const
    {
        if (this->_iX != other._iX)
            return this->_iX < other._iX;
        return this->_iY < other._iY;
    }
    bool operator==(const st_Sector_Pos &other) const
    {
        return this->_iX == other._iX && this->_iY == other._iY;
    }
    int _iX, _iY;
};
struct st_Sector_Around
{
    st_Sector_Around()
    {
        // Around.reserve(6);
    }
    std::set<st_Sector_Pos> Around;
};

#include <algorithm>

#define SectorMax dfRANGE_MOVE_BOTTOM / dfSECTOR_Size

class SectorManager
{
  public:
    static void GetSectorAround(int iX, int iY,
                                st_Sector_Around *pSectorAround)
    {

        for (int row = -1; row <= 1; row++)
        {
            for (int column = -1; column <= 1; column++)
            {
                int rx = std::clamp(iX + row, 0, SectorMax - 1);
                int ry = std::clamp(iY + column, 0, SectorMax - 1);
                st_Sector_Pos temp;
                temp._iX = rx;
                temp._iY = ry;
                pSectorAround->Around.insert(temp);
            }
        }
    }
};