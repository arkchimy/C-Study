#pragma once
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

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
#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
#define MAX_SESSION_COUNT 7000
using ull = unsigned long long;

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


class CLanServer
{
  public:
    CLanServer();
    ~CLanServer();

    // 오픈 IP / 포트 / 제로카피 여부 /워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    virtual BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions);
    void Stop();

    bool Disconnect(const ull SessionID);
    bool DisconnectForContents(const ull SessionID);

    CMessage *CreateMessage(class clsSession *const session, class stHeader &header);
    //CMessage *CreateLoginMessage();
    char *CreateLoginMessage();

    void RecvComplete(class clsSession *const session, DWORD transferred);
    void SendComplete(class clsSession *const session, DWORD transferred);

    void SendPacket(class clsSession *const session);
    void RecvPacket(class clsSession *const session);

    virtual bool OnAccept(ull SessionID) = 0;
    virtual double OnRecv(ull SessionID, CMessage *msg) = 0;

    int GetSessionCount();
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

    LONG64 m_SessionCount = 0;
    ull iDisCounnectCount = 0;

    std::vector<class clsSession> sessions_vec;
    CLockFreeStack<ull> m_IdxStack; // 반환된 Idx를 Stack형식으로 
    //CLockFreeQueue<clsSession *> m_ReleaseSessions;
    CLockFreeStack<clsSession *> m_ReleaseSessions;
    int m_WorkThreadCnt = 0; // MonitorThread에서 WorkerThread의 갯수를 알기위한 변수.
    DWORD m_tlsIdxForTPS = 0;
    LONG64 *arrTPS = nullptr; //  idx 0 : Contents , 나머지 : WorkerThread들이 측정할 Count변수의 동적 배열

    HANDLE WorkerArg[2]{0}; // WorkerThread __beginthreadex 매개변수
    HANDLE AcceptArg[3]{0};  // AcceptThread __beginthreadex 매개변수

    static CSystemLog *systemLog;
};
