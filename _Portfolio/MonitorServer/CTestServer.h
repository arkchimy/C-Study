#pragma once
#include "./CNetworkLib/CNetworkLib.h"
#include <thread>

#include <unordered_map>

enum class enClientType : uint8_t
{
    LoginServer,
    ChatServer,
    GameServer,
    MonitorClient, // 다른 프로토콜로 접속

    Max,
};


struct stPlayer
{
    enClientType m_type = enClientType::Max;
    int m_ServerNo = 0;
    ull m_sessionID = 0;

    DWORD m_Timer = 0;

    char m_ipAddress[16];
    wchar_t m_wipAddress[16];

    USHORT m_port;


    int32_t DataSet[dfMONITOR_DATA_TYPE_MONITOR_MAX];
    // 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니 int로 캐스팅 38년까지만 사용가능
    int32_t timeStamp; 
};

class CTestServer : public CLanServer
{
  public:
    CTestServer(bool EnCoding = false);
    void MonitorThread();

    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);


    virtual bool OnAccept(ull SessionID, SOCKADDR_IN &addr) override ;
    virtual void OnRecv(ull SessionID, struct CMessage *msg) override;
    virtual void OnRelease(ull SessionID) override;


    virtual void REQ_MONITOR_LOGIN(ull SessionID, CMessage *msg, int ServerNo, WORD wType = en_PACKET_SS_MONITOR_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    virtual void REQ_MONITOR_UPDATE(ull SessionID, CMessage *msg, BYTE DataType, int DataValue, int TimeStamp, WORD wType = en_PACKET_SS_MONITOR_DATA_UPDATE, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);
    virtual void REQ_MONITOR_TOOL_LOGIN(ull SessionID, CMessage *msg, WCHAR *LoginSessionKey, WORD wType = en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN, BYTE bBroadCast = false, std::vector<ull> *pIDVector = nullptr, size_t wVectorLen = 0);

        // SessionID Key , Player접근.

private:
    std::unordered_map<ull, stPlayer *> SessionID_hash; // LoginPack을 받은 Session.
    std::unordered_map<int, stPlayer *> ServerNo_hash;  // 중복 접속을 제거하는 용도
    std::unordered_map<ull, stPlayer *> waitLogin_hash; // LoginPack을 받기 전 Session.


    // 락을 잡은상태에서 잡을경우 선언 순서대로 잡기.
    SharedMutex SessionID_hash_Lock; // SessionID_hash 이용도로 씀.
    SharedMutex ServerNo_hash_Lock;  // ServerNo_hash  이용도로 씀.
    SharedMutex waitLogin_hash_Lock; // waitLogin_hash 이용도로 씀.

    
    CObjectPool_UnSafeMT<stPlayer> player_pool;
    CObjectPool<stDBOverlapped> dbOverlapped_pool;

    WinThread _hMonitorThread;

    bool bOn = true;


    wchar_t _bindAddr[16] = {0,};
    unsigned short _port;
    int _ZeroCopy;
    int _WorkerCreateCnt;
    int _reduceThreadCount;
    int _noDelay;
    int _MaxSessions;

};
