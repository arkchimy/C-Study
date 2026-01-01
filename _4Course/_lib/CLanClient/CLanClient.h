#pragma once
#define WIN32_LEAN_AND_MEAN

#include <WS2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <queue>
#include <thread>
#include <vector>

#include <stack>
#include "Include_utility.h"


using ull = unsigned long long;

class CLanClient
{
    enum
    {
        SessionMax = 10000,

    };

  public:
    CLanClient(bool bEnCording = false);

    
    CMessage *CreateMessage(class clsSession &session, struct stHeader &header);

    void WorkerThread();

    bool Connect(const wchar_t *ServerAddress, short Serverport,const wchar_t *BindipAddress = nullptr, int workerThreadCnt = 1, int bNagle = true, int reduceThreadCount = 0, int userCnt = 1); // 바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
    bool Disconnect(const ull SessionID);
    void CancelIO_Routine(const ull SessionID); // Session에 대한 안정성은  외부에서 보장해주세요.
    
    void RecvPacket(class clsSession &session);
    void SendPacket(ull SessionID, CMessage *msg, BYTE SendType,
                    std::vector<ull> *pIDVector, WORD wVecLen);
    void Unicast(ull SessionID, CMessage *msg, LONG64 Account = 0);
    void BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, WORD wVecLen);

    void RecvComplete(class clsSession &session, DWORD transferred);
    void SendComplete(class clsSession &session, DWORD transferred);
    void ReleaseComplete(ull SessionID);
    void ReleaseSession(ull SessionID);

    void WSASendError(const DWORD LastError, const ull SessionID);
    void WSARecvError(const DWORD LastError, const ull SessionID);


    bool SessionLock(ull SessionID);   // 내부에서 IO를 증가시켜 안전을 보장함.
    void SessionUnLock(ull SessionID); // 반환형 쓸때가 없음.

    virtual void OnEnterJoinServer(ull SessionID) = 0; //	< 서버와의 연결 성공 후
    virtual void OnLeaveServer(ull SessionID) = 0;     //< 서버와의 연결이 끊어졌을 때

    virtual float OnRecv(ull SessionID, CMessage *msg) = 0; //< 하나의 패킷 수신 완료 후
    virtual void OnSend(int sendsize) = 0; //	< 패킷 송신 완료 후
    virtual void OnRelease(ull SessionID) = 0;

  private:
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
    std::vector<SOCKET> _sockVec;
    std::vector<std::thread> _hIocpThreadVec;

    //Debuggin정보
    LONG64 *arrTPS = nullptr; //  idx 0 : AcceptTps , 나머지 : WorkerThread들이 측정할 Count변수의 동적 배열
    ull iDisCounnectCount = 0;

    ////////////////

    clsSession sessions_vec[SessionMax];
    CLockFreeStack<ull> _IdxStack;
    ull g_ID = 0; //  [ IDX :17  SEQ : 47 ]

    // Start에서 초기화

     DWORD m_tlsIdxForTPS = 0; 
    int headerSize = 0;
     bool bEnCording = false;
    //////////////
};
