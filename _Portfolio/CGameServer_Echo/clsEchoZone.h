#pragma once
#include "CZoneNetworkLib/CZoneNetworkLib.h"
#include <unordered_map>
#include "enZoneType.h"
class clsEchoZone : public IZone
{
    enum enMsgType : std::uint8_t
    {
        EchoPacket,
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

    // Move완료 패킷이 없어서 여기서 로그인 성공을 보냄.
    void RES_LOGIN(ull SessionID, CMessage *msg, BYTE Status, INT64 AccountNo, WORD wType = en_PACKET_CS_GAME_RES_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

    void REQ_ECHO(ull SessionID, CMessage *msg, INT64 AccountNo, LONGLONG SendTick, WORD wType = en_PACKET_CS_GAME_REQ_ECHO, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    void RES_ECHO(ull SessionID, CMessage *msg, INT64 AccountNo, LONGLONG SendTick, WORD wType = en_PACKET_CS_GAME_RES_ECHO, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

    void REQ_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType = en_PACKET_CS_GAME_REQ_HEARTBEAT, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    void RES_HEARTBEAT(ull SessionID, CMessage *msg, WORD wType = en_PACKET_CS_GAME_REQ_HEARTBEAT, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

  public:

    // 하트비트를 돌기위한 자료구조.
    std::unordered_map<ull, stPlayer *> SessionID_hash;

    DWORD _sessionTimeoutMs = 60000;

    ull _msgTypeCntArr[Max]{0,};
    ull totalPacketCnt = 0;
};