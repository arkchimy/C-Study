#pragma once
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WS2tcpip.h>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <vector>
#include <thread>
#include <queue>
#include "CRingBuffer.h"

using ull = unsigned long long;

struct CMessage;

enum class Job_Type : BYTE
{
    Recv,
    Send,
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

    stOverlapped m_recvOverlapped = stOverlapped(Job_Type::Recv);
    stSendOverlapped m_sendOverlapped = stSendOverlapped(Job_Type::Send);

    std::queue<CMessage *> m_sendBuffer;
    CRingBuffer m_recvBuffer; 

    ull m_ioCount = 0;
    int id;
    SOCKET _sock;
};
class CLanClient
{
  public:
    CLanClient(bool EnCoding = false);


    void WorkerThread();

    bool Connect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress = nullptr, int workerThreadCnt = 1, int bNagle = true, int reduceThreadCount = 0 , int userCnt = 1); // 바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
    bool Disconnect();
    bool SendPacket(CMessage *msg);

    void RecvComplete(class clsSession &session, DWORD transferred);
    void SendComplete(class clsSession &session, DWORD transferred);

    virtual void OnEnterJoinServer() = 0; //	< 서버와의 연결 성공 후
    virtual void OnLeaveServer() = 0;     //< 서버와의 연결이 끊어졌을 때

    virtual void OnRecv(CMessage *) = 0;   //< 하나의 패킷 수신 완료 후
    virtual void OnSend(int sendsize) = 0; //	< 패킷 송신 완료 후
  private:
    HANDLE _hIOCP = INVALID_HANDLE_VALUE;
    std::vector<SOCKET> _sockVec;
    std::vector<std::thread> _hIocpThreadVec;
    std::vector<clsSession> _sessionVec;
};
