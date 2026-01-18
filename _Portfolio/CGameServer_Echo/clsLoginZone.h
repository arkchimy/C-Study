#pragma once
#include "CZoneNetworkLib/CZoneNetworkLib.h"
#include <unordered_map>
#include "enZoneType.h"

/*
* 
*     // 해당 Hash 는 다른 Zone에서도 접근함.
    std::unordered_map<ull, stPlayer *> SessionID_hash;
    // 중복 로그인 주의

    std::unordered_map<INT64, stPlayer *> Account_hash;

    이 부분을 Server 함수를 통해 전달하여 Lock을 걸고 접근하는 방식을 제거.
*/
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
    virtual void OnDisConnect(ull SessionID);


    bool PacketProc(ull SessionID, CMessage *msg);

    void REQ_LOGIN(ull SessionID, CMessage *msg, INT64 AccountNo, WCHAR *SessionKey, WORD wType = en_PACKET_CS_GAME_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);


 
    // LoginPack이 두번 올 가능성.
    SharedMutex _SessionTable_Mutex;

    // 해당 Hash 는 다른 Zone에서도 접근함.
    std::unordered_map<ull, stPlayer *> SessionID_hash;
    // 중복 로그인 주의

    std::unordered_map<INT64, stPlayer *> Account_hash;
    // 인증전  SessionID와 시간
    std::unordered_map<ull, stPlayer *> prePlayer_hash;

    CObjectPool<stPlayer> player_pool;

    private:
    DWORD _sessionTimeoutMs = 10000;   // 10초
    
  public:
    ull _msgTypeCntArr[Max]{0,};
    ull totalPacketCnt = 0;
};