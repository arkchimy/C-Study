#pragma once
#include "./CNetworkLib/CNetworkLib.h"
#include <thread>

#include <unordered_map>

enum class ClientType : uint8_t
{
    LoginServer,
    ChatServer,
    GameServer,
    MonitorClient, // 다른 프로토콜로 접속

    Max,
};


struct stPlayer
{
    ClientType m_type = ClientType::Max;
    int m_ServerNo = 0;
    ull m_sessionID = 0;

    DWORD m_Timer = 0;

    char m_ipAddress[16];
    USHORT m_port;
};

class CTestServer : public CLanServer
{
  public:
    CTestServer(bool EnCoding = false);

    virtual bool OnAccept(ull SessionID, SOCKADDR_IN &addr) override ;
    virtual void OnRecv(ull SessionID, struct CMessage *msg, bool bBalanceQ = false) override;
    virtual void OnRelease(ull SessionID) override;


    virtual void REQ_LOGIN_Server(ull SessionID, CMessage *msg, int ServerNo, WORD wType = en_PACKET_SS_MONITOR_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0) ;
    virtual void REQ_DATA_UPDATE(ull SessionID, CMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType = en_PACKET_SS_MONITOR_DATA_UPDATE, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    virtual void REQ_LOGIN_Client(ull SessionID, CMessage *msg, WCHAR *LoginSessionKey, WORD wType = en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    virtual void REQ_MONITOR_TOOL_UPDATE(ull SessionID, CMessage *msg, BYTE ServerNo, BYTE DataType, int DataValue, int TimeStamp, WORD wType = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

        // SessionID Key , Player접근.
    std::unordered_map<ull, stPlayer *> SessionID_hash; // 중복 접속을 제거하는 용도


    CObjectPool_UnSafeMT<stPlayer> player_pool;
    CObjectPool<stDBOverlapped> dbOverlapped_pool;
    SharedMutex SessionID_hash_Lock;

};
