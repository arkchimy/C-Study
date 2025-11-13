#pragma once

#include "Stub.h"
#include "Proxy.h"

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")


#include <list>
#include <map>
#include <vector>

#include "../SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../CLockFreeStack_lib/CLockFreeStack.h"
#include "../CLockFreeQueue_lib/CLockFreeQueue_lib.h"
#include "../CSystemLog_lib/CSystemLog_lib.h"

#include "clsSession.h"
#include "stHeader.h"




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
            ERROR_FILE_LOG(L"WSAData_Error.txt", L"WSAStartup retval is not Zero ");
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


class CLanServer : public Stub, public  Proxy
{
  public:
    CLanServer(bool EnCoding = false);
    ~CLanServer();

    // 오픈 IP / 포트 / 제로카피 여부 /워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    void Stop();

    bool Disconnect(const ull SessionID);
    void CancelIO_Routine(const ull SessionID); // Session에 대한 안정성은  외부에서 보장해주세요.

    void DecrementIoCountAndMaybeDeleteSession(clsSession &session);
    CMessage *CreateMessage(class clsSession &session, class stHeader &header);
    // CMessage *CreateLoginMessage();
    char *CreateLoginMessage();

    // void RecvComplete(class clsSession *const session, DWORD transferred);
    void RecvComplete(class clsSession &session, DWORD transferred);
    void SendComplete(class clsSession &session, DWORD transferred);

    bool SessionLock(ull SessionID);   // 내부에서 IO를 증가시켜 안전을 보장함.
    void SessionUnLock(ull SessionID); // 반환형 쓸때가 없음.

    void SendPacket(ull SessionID, struct CMessage *msg, BYTE SendType,
                    int iSectorX = 0, int iSectorY = 0);
    void UnitCast(ull SessionID, CMessage *msg, size_t size);

    void RecvPacket(class clsSession &session);

    virtual bool OnAccept(ull SessionID) = 0;
    virtual float OnRecv(ull SessionID, struct CMessage *msg) = 0;
    virtual void OnRelease(ull SessionID) = 0;

    LONG64 GetSessionCount();
    virtual LONG64 GetPlayerCount() { return 0; } // Contents에서 구현하기.
    LONG64 GetReleaseSessions();
    LONG64 Get_IdxStack();

    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();


    void WSASendError(const DWORD LastError, const ull SessionID);
    void WSARecvError(const DWORD LastError, const ull SessionID);
    void ReleaseSession(ull SessionID);

  public:
    SOCKET m_listen_sock = INVALID_SOCKET;
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
    HANDLE *m_hThread = nullptr;

    HANDLE m_hAccept = INVALID_HANDLE_VALUE;

    bool bZeroCopy = false;
    bool bOn = false;
    bool bEnCording = false;

    LONG64 m_SessionCount = 0;
    ull iDisCounnectCount = 0;
    int headerSize = 0;

    std::vector<class clsSession> sessions_vec;

    CLockFreeStack<ull> m_SessionIdxStack; // 반환된 Idx를 Stack형식으로

    // CLockFreeQueue<clsSession *> m_ReleaseSessions;
    CLockFreeStack<clsSession *> m_ReleaseSessions;
    int m_WorkThreadCnt = 0; // MonitorThread에서 WorkerThread의 갯수를 알기위한 변수.
    DWORD m_tlsIdxForTPS = 0;
    LONG64 *arrTPS = nullptr; //  idx 0 : Contents , 나머지 : WorkerThread들이 측정할 Count변수의 동적 배열

    HANDLE WorkerArg[2]{0}; // WorkerThread __beginthreadex 매개변수
    HANDLE AcceptArg[3]{0}; // AcceptThread __beginthreadex 매개변수
    
    static CSystemLog *systemLog;

    LONG64 m_AllocMsgCount = 0;
    int m_AllocLimitCnt = 10000;
};
