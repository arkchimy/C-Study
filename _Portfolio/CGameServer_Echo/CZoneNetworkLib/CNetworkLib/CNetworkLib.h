#pragma once

#include "RPC/Proxy.h"
#include "RPC/Stub.h"

#include "utility/clsSession.h"
#include "utility/stHeader.h"

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <vector>

#include "utility/CLockFreeQueue/CLockFreeQueue.h"
#include "utility/CLockFreeStack/CLockFreeStack.h"
#include "utility/CSystemLog/CSystemLog.h"
#include "utility/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "utility/Parser/Parser.h"
#include "utility/CTlsObjectPool/CTlsObjectPool.h"

#include "utility/Profiler_MultiThread/Profiler_MultiThread.h"
#include "utility/DeadLockGuard/DeadLockGuard_lib.h"
#include "WinAPI/WinThread.h"
#include "WinAPI/Atomic.h"


using ull = unsigned long long;

struct stWSAData
{
  // main에서 선언
  public:
    stWSAData()
    {
        WSAData wsa;
        DWORD wsaStartRetval;

        wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (wsaStartRetval != 0)
        {
            __debugbreak();
        }
    }
    ~stWSAData()
    {
        WSACleanup();
    }
};

class CLanServer : public Stub, public Proxy
{
    friend class Stub;
    friend class Proxy;
  private:
    void WorkerThread();
    void AcceptThread();
  public:
    CLanServer(bool EnCoding = false);
    virtual ~CLanServer();

    // 오픈 IP / 포트 / 제로카피 여부 /워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    
    // Lock을 소유한 곳에서는 호출 금지. SignalOnForStop의 이벤트를 대기함.
    virtual void Stop();           

    // Player가 0으로 떨어졌을때 반드시 호출 해줘야 함.
    virtual void SignalOnForStop();

    bool Disconnect(const ull SessionID);
    void CancelIO_Routine(const ull SessionID); // Session에 대한 안정성은  외부에서 보장해주세요.

    void DecrementIoCountAndMaybeDeleteSession(clsSession &session);
    CMessage *CreateMessage(class clsSession &session, struct stHeader &header) const ;

    // void RecvComplete(class clsSession *const session, DWORD transferred);
    void RecvComplete(class clsSession &session, DWORD transferred);
    void SendComplete(class clsSession &session, DWORD transferred);
    void ReleaseComplete(clsSession& session);

    bool SessionLock(ull SessionID);   // 내부에서 IO를 증가시켜 안전을 보장함.
    void SessionUnLock(ull SessionID); // 반환형 쓸때가 없음.

    void SendPacket(ull SessionID, struct CMessage *msg, BYTE SendType,
                    std::vector<ull> *pIDVector = nullptr, size_t wVecLen = 0);
    void Unicast(ull SessionID, CMessage *msg, LONG64 Account = 0);
    void BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, size_t wVecLen);

    void RecvPacket(class clsSession &session);

    virtual bool OnAccept(ull SessionID , SOCKADDR_IN& addr) = 0;
    virtual void OnRecv(ull SessionID, struct CMessage *msg) = 0;
    virtual void OnRelease(ull SessionID) = 0;

    LONG64 GetSessionCount() const ; 
    virtual LONG64 GetPlayerCount() { return 0; } // Contents에서 구현하기.
    LONG64 Get_IdxStack() const ;

    ull getTotalAccept() const { return m_TotalAccept; }
    ull getNetworkMsgCount() const { return m_NetworkMsgCount; }

    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();

    void WSASendError(const DWORD LastError, const ull SessionID);
    void WSARecvError(const DWORD LastError, const ull SessionID);
    void ReleaseSession(ull SessionID);

    void PushSessionStack(ull SessionID);

    clsSession *GetSession(ull SessionID) 
    {
        return &sessions_vec[SessionID >> 47];
    }
  protected:
    // SignalOnForStop에서 사용할 이벤트객체
    HANDLE hReadyForStopEvent = INVALID_HANDLE_VALUE; 

    std::vector<clsSession> sessions_vec;
    SOCKET m_listen_sock = INVALID_SOCKET;
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;

    std::vector<WinThread> m_hWorkerThread;
    WinThread m_hAccept;

    bool bZeroCopy = false;
    bool bOn = false;

    int bNoDelay = false;

    LONG64 m_SessionCount = 0;
    ull iDisCounnectCount = 0;



    CLockFreeStack<ull> m_SessionIdxStack; // 반환된 Idx를 Stack형식으로
    int m_WorkThreadCnt = 0;               // MonitorThread에서 WorkerThread의 갯수를 알기위한 변수.

    std::vector<LONG64> arrTPS;

    ull m_TotalAccept = 0;
    LONG64 m_AllocMsgCount = 0;
    LONG64 m_NetworkMsgCount = 0;

    int m_AllocLimitCnt = 10000;

    bool bEnCording = false;
    int headerSize = 0;
};
