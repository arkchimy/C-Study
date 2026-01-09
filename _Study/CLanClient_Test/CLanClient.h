#pragma once
#define WIN32_LEAN_AND_MEAN

#include "../../_Portfolio/MonitorServer/CNetworkLib/CNetworkLib.h"

using ull = unsigned long long;

class CLanClient
{
    enum
    {
        SessionMax = 10000,

    };
  public:
    CLanClient(bool EnCoding = false);


    void WorkerThread();

    bool Connect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress = nullptr, int workerThreadCnt = 1, int bNagle = true, int reduceThreadCount = 0 , int userCnt = 1); // 바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
    bool Disconnect();

     CMessage *CreateMessage(class clsSession &session, struct stHeader &header) const;

    void RecvPacket(class clsSession &session);
    void SendPacket( CMessage *msg, BYTE SendType,
                    std::vector<ull> *pIDVector, WORD wVecLen);
    void Unicast(CMessage *msg, LONG64 Account = 0);

    void RecvComplete( DWORD transferred );
    void SendComplete( DWORD transferred );
    void ReleaseComplete();
    void ReleaseSession();

    void WSASendError(const DWORD LastError, const ull SessionID = 0);
    void WSARecvError(const DWORD LastError, const ull SessionID = 0);

    bool SessionLock(ull SessionID);   // 내부에서 IO를 증가시켜 안전을 보장함.
    void SessionUnLock(ull SessionID); // 반환형 쓸때가 없음.


    virtual void OnEnterJoinServer() = 0; //	< 서버와의 연결 성공 후
    virtual void OnLeaveServer() = 0;     //< 서버와의 연결이 끊어졌을 때

    virtual void OnRecv(CMessage *) = 0;   //< 하나의 패킷 수신 완료 후
    virtual void OnRelease() = 0;
  private:
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
    
    std::vector<std::thread> _hIocpThreadVec;

    clsSession session;

    ull g_ID = 0; //  [ IDX :17  SEQ : 47 ]

    bool bEnCording = false;
    int headerSize = 0;

};
