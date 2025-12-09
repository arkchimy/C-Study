#pragma once

#include "RPC/Proxy.h"
#include "RPC/Stub.h"

#include "utility/clsSession.h"
#include "utility/stHeader.h"

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <list>
#include <map>
#include <vector>

#include "utility/CLockFreeQueue/CLockFreeQueue.h"
#include "utility/CLockFreeStack/CLockFreeStack.h"
#include "utility/CSystemLog/CSystemLog.h"
#include "utility/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "utility/Parser/Parser.h"
#include "utility/CTlsObjectPool/CTlsObjectPool.h"
#include "utility/Profiler_MultiThread/Profiler_MultiThread.h"

using ull = unsigned long long;

class st_WSAData
{
  public:
    st_WSAData()
    {
        WSAData wsa;
        DWORD wsaStartRetval;

        wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (wsaStartRetval != 0)
        {
            __debugbreak();
        }
    }
    ~st_WSAData()
    {
        WSACleanup();
    }
};

struct stSRWLock
{
    stSRWLock(SRWLOCK *srw)
        : m_srw(srw)
    {
        AcquireSRWLockExclusive(m_srw);
    }
    ~stSRWLock()
    {
        ReleaseSRWLockExclusive(m_srw);
    }
    SRWLOCK *m_srw;
};

class CLanServer : public Stub, public Proxy
{
  public:
    CLanServer(bool EnCoding = false);
    ~CLanServer();

    // 오픈 IP / 포트 / 제로카피 여부 /워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    void Stop();
    void DecrementIoCountAndMaybeDeleteSession(clsSession &session);

    CMessage *CreateMessage(class clsSession &session, struct stHeader &header);

    // void RecvComplete(class clsSession *const session, DWORD transferred);
    void RecvComplete(class clsSession &session, DWORD transferred);
    void SendComplete(class clsSession &session, DWORD transferred);

    bool SessionLock(ull SessionID);   // 내부에서 IO를 증가시켜 안전을 보장함.
    void SessionUnLock(ull SessionID); // 반환형 쓸때가 없음.

    void SendPacket(ull SessionID, struct CMessage *msg, BYTE SendType,
                    std::vector<ull> *pIDVector = nullptr, WORD wVecLen = 0);
    void Unicast(ull SessionID, CMessage *msg, LONG64 Account = 0);
    void BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, WORD wVecLen);

    void RecvPacket(class clsSession &session);

    public:
    bool Disconnect(const ull SessionID);
    void CancelIO_Routine(const ull SessionID); // Session에 대한 안정성은  외부에서 보장해주세요.
    virtual bool OnAccept(ull SessionID) = 0;
    virtual float OnRecv(ull SessionID, struct CMessage *msg, bool bBalanceQ = false) = 0;
    virtual void OnRelease(ull SessionID) = 0;

    LONG64 GetSessionCount();
    virtual LONG64 GetPlayerCount() { return 0; } // Contents에서 구현하기.
    LONG64 Get_IdxStack();

    ull getTotalAccept() { return m_TotalAccept; }
    ull getNetworkMsgCount() { return m_NetworkMsgCount; }
    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();

    void WSASendError(const DWORD LastError, const ull SessionID);
    void WSARecvError(const DWORD LastError, const ull SessionID);
    void ReleaseSession(ull SessionID);

    SOCKET m_listen_sock = INVALID_SOCKET;
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
    HANDLE *m_hWorkerThread = nullptr;

    HANDLE m_hAccept = INVALID_HANDLE_VALUE;

    bool bZeroCopy = false;
    bool bEnCording = false;
    int bNoDelay = false;

    LONG64 m_SessionCount = 0;
    ull iDisCounnectCount = 0;
    int headerSize = 0;

    std::vector<class clsSession> sessions_vec;

    CLockFreeStack<ull> m_SessionIdxStack; // 반환된 Idx를 Stack형식으로
    int m_WorkThreadCnt = 0;               // MonitorThread에서 WorkerThread의 갯수를 알기위한 변수.

    DWORD m_tlsIdxForTPS = 0; // Start에서 초기화
    LONG64 *arrTPS = nullptr; //  idx 0 : AcceptTps , 나머지 : WorkerThread들이 측정할 Count변수의 동적 배열

    HANDLE WorkerArg[2]{0}; // WorkerThread __beginthreadex 매개변수
    HANDLE AcceptArg[3]{0}; // AcceptThread __beginthreadex 매개변수

    ull m_TotalAccept = 0;
    LONG64 m_AllocMsgCount = 0;
    LONG64 m_NetworkMsgCount = 0;

    int m_AllocLimitCnt = 10000;

  public:
    bool bOn = false;

};
