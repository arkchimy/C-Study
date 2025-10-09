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

    bool Disconnect(class clsSession *const session);

    CMessage *CreateCMessage(class clsSession *const session, class stHeader &header);
    CMessage *CreateLoginMessage();

    void RecvComplete(class clsSession *const session, DWORD transferred);
    void SendComplete(class clsSession *const session, DWORD transferred);

    void SendPacket(class clsSession *const session);
    void RecvPacket(class clsSession *const session);

    virtual bool OnAccept(ull SessionID) = 0;
    virtual double OnRecv(ull SessionID, CMessage *msg) = 0;

    int GetSessionCount();

    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();

  public:
    SOCKET m_listen_sock = INVALID_SOCKET;
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
    HANDLE *m_hThread = nullptr;

    HANDLE m_hAccept = INVALID_HANDLE_VALUE;

    bool bZeroCopy = false;
    bool bOn = false;

    std::vector<class clsSession> sessions_vec;

    // TODO : LockFree Stack으로 대체하기
    std::list<ull> m_idleIdx;
    // SRWLOCK srw_session_idleList;

    int m_WorkThreadCnt = 0; // MonitorThread에서 WorkerThread의 갯수를 알기위한 변수.
    DWORD m_tlsIdxForTPS;
    LONG64 *arrTPS; //  idx 0 : Contents , 나머지 : WorkerThread들이 측정할 Count변수의 동적 배열

    HANDLE WorkerArg[2]; // WorkerThread __beginthreadex 매개변수
    HANDLE AcceptArg[3]; // AcceptThread __beginthreadex 매개변수
};
