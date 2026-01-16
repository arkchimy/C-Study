#pragma once
#include "CZoneNetworkLib/CZoneNetworkLib.h"
#include <unordered_map>
#include "enZoneType.h"

class clsLoginZone : public IZone
{
    enum enMsgType : std::uint8_t
    {
        LoginPacket,
        HeartBeatPacket,
        Max,
    };

  public:
    // 이 함수를 컨텐츠 개발자가 구현해야함.
    virtual void OnEnterWorld(ull SessionID, SOCKADDR_IN &addr);
    virtual void OnRecv(ull SessionID, struct CMessage *msg);
    virtual void OnUpdate();
    virtual void OnLeaveWorld(ull SessionID);

    bool PacketProc(ull SessionID, CMessage *msg);

    void REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType = en_PACKET_CS_GAME_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

    void RES_LOGIN(ull SessionID, CMessage *msg, BYTE Status, INT64 AccountNo, WORD wType = en_PACKET_CS_GAME_RES_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

    void REQ_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType = en_PACKET_CS_GAME_REQ_HEARTBEAT, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    void RES_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType = en_PACKET_CS_GAME_REQ_HEARTBEAT, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);


 
    // LoginPack이 두번 올 가능성.
    SharedMutex _SessionTable_Mutex;

    // 해당 Hash 는 다른 Zone에서도 접근함.
    std::unordered_map<ull, stPlayer *> SessionID_hash;
    // 중복 로그인 주의
    std::unordered_map<INT64, stPlayer *> Account_hash;

    // 인증전  SessionID와 시간
    std::unordered_map<ull, stPlayer *> prePlayer_hash;

    private:
    DWORD _prePlayerTimeoutMs = 10000; //10초
    DWORD _sessionTimeoutMs = 60000;   // 60초
    
  public:
    ull _msgTypeCntArr[Max]{0,};
    ull totalPacketCnt = 0;
};