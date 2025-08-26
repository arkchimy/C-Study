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
#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

class st_WSAData
{
  public:
    st_WSAData();
    ~st_WSAData();
};
using ull = unsigned long long;

struct stSRWLock
{
    stSRWLock(SRWLOCK& srw)
        : m_srw(srw)
    {
        AcquireSRWLockExclusive(&m_srw);
    }
    ~stSRWLock()
    {
        ReleaseSRWLockExclusive(&m_srw);
    }
    SRWLOCK m_srw;
};


class CLanServer
{
  public:
    ~CLanServer();
    // 오픈 IP / 포트 / 워커스레드 수 (생성수, 러닝수) / 나글옵션 / 최대접속자 수
    BOOL Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions, int ZeroByteTest);
    void Stop();
    int GetSessionCount();
    bool Disconnect(class clsSession *const session);

    void RecvComplete(class clsSession *const session, DWORD transferred);
    CMessage *CreateCMessage(class clsSession *const session, class stHeader &header);

    void SendComplete(class clsSession *const session, DWORD transferred);
    void PostComplete(class clsSession *const session, DWORD transferred);

    void SendPacket(class clsSession *const session);
    void RecvPacket(class clsSession *const session);

    virtual double OnRecv(ull SessionID, CMessage *msg) = 0;
    virtual void SendPostMessage(ull SessionID) = 0;

    /*
            virtual bool OnConnectionRequest(IP, Port) = 0; //
        < accept 직후

        return false; 시 클라이언트 거부.
        return true;  시 접속 허용

            virtual void OnAccept(Client 정보 / SessionID / 기타등등) = 0;
        < Accept 후 접속처리 완료 후 호출.

            virtual void OnRelease(SessionID) = 0;
        < Release 후 호출

        //	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
        //	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후

        virtual void OnError(int errorcode, wchar *) = 0;

        */
    int getAcceptTPS();
    int getRecvMessageTPS();
    int getSendMessageTPS();

  public:
    SOCKET m_listen_sock;
    HANDLE m_hIOCP;
    HANDLE *m_hThread;
    HANDLE m_hAccept;
    HANDLE m_ContentsEvent;
    HANDLE m_ServerOffEvent;
    int m_ZeroByteTest;
    bool bZeroCopy = false;
    std::map<ull, class clsSession *> sessions;

    std::vector<class clsSession> sessions_vec;
    std::list<ull> m_idleIdx;
    // TODO : LockFree Q 가 올 때까지 사용
    SRWLOCK srw_session_idleList;
};
