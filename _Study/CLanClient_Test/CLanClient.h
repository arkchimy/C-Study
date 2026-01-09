#pragma once
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WS2tcpip.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <vector>
#include <thread>
#include <queue>

#include <stack>

#include "../../_3Course/lib/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../../_3Course/lib/Profiler_MultiThread/Profiler_MultiThread.h"
#include "../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"

using ull = unsigned long long;

enum class Job_Type : BYTE
{
    Recv,
    Send,
    ReleasePost,
    MAX,
};
// _mode 판단을 stOverlapped 기준으로 하므로 첫 멤버변수 _mode 로 할것.
struct stOverlapped : public OVERLAPPED
{
    stOverlapped(Job_Type mode) : _mode(mode) {}
    Job_Type _mode = Job_Type::MAX;
};

struct stSendOverlapped : public OVERLAPPED
{
    stSendOverlapped(Job_Type mode) : _mode(mode) {}
    Job_Type _mode = Job_Type::MAX;
    DWORD msgCnt = 0;
    struct CMessage *msgs[500]{
        0,
    };
};
struct clsSession
{

    void Release();

    stOverlapped m_recvOverlapped = stOverlapped(Job_Type::Recv);
    stSendOverlapped m_sendOverlapped = stSendOverlapped(Job_Type::Send);
    stOverlapped m_releaseOverlapped = stOverlapped(Job_Type::ReleasePost);

    std::queue<CMessage *> m_sendBuffer;
    CRingBuffer m_recvBuffer; 

    ull m_ioCount = 0;

    ull _sessionID;
    ull m_flag;
    SOCKET _sock;
};
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

    void SendPacket(ull SessionID, CMessage *msg, BYTE SendType,
                    std::vector<ull> *pIDVector, WORD wVecLen);
    void Unicast(ull SessionID, CMessage *msg, LONG64 Account = 0);
    void BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, WORD wVecLen);

    void RecvComplete(class clsSession &session, DWORD transferred);
    void SendComplete(class clsSession &session, DWORD transferred);
    void ReleaseComplete(ull SessionID);
    void ReleaseSession(ull SessionID);

    bool SessionLock(ull SessionID);   // 내부에서 IO를 증가시켜 안전을 보장함.
    void SessionUnLock(ull SessionID); // 반환형 쓸때가 없음.


    virtual void OnEnterJoinServer(ull SessionID) = 0; //	< 서버와의 연결 성공 후
    virtual void OnLeaveServer(ull SessionID) = 0;     //< 서버와의 연결이 끊어졌을 때

    virtual void OnRecv(CMessage *) = 0;   //< 하나의 패킷 수신 완료 후
    virtual void OnSend(int sendsize) = 0; //	< 패킷 송신 완료 후
    virtual void OnRelease(ull SessionID) = 0;
  private:
    HANDLE m_hIOCP = INVALID_HANDLE_VALUE;
    std::vector<SOCKET> _sockVec;
    std::vector<std::thread> _hIocpThreadVec;

    clsSession sessions_vec[SessionMax];
    std::stack<ull> _IdxStack; 

    ull g_ID = 0; //  [ IDX :17  SEQ : 47 ]

    bool bEnCording = false;
    int headerSize = 0;

};
