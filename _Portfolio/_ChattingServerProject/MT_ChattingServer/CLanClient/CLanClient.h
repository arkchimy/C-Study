#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

using ull = unsigned long long;
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
#include "utility/CTlsObjectPool/CTlsObjectPool.h"
#include "utility/Parser/Parser.h"
#include "utility/SerializeBuffer_exception/SerializeBuffer_exception.h"

#include "WinAPI/Atomic.h"
#include "WinAPI/WinThread.h"
#include "utility/DeadLockGuard/DeadLockGuard_lib.h"
#include "utility/Profiler_MultiThread/Profiler_MultiThread.h"

class CLanClient : public Stub, public Proxy
{
    friend class Stub;
    friend class Proxy;
  public:
    CLanClient(bool EnCoding = false);

    void WorkerThread();

    bool Connect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress = nullptr, int workerThreadCnt = 1, int bNagle = true, int reduceThreadCount = 0, int userCnt = 1); // 바인딩 IP, 서버IP / 워커스레드 수 / 나글옵션
    // 재연결
    bool ReConnect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress = nullptr); 
    void Disconnect(ull SessionID);

    CClientMessage *CreateMessage(class clsSession &session, struct stHeader &header) const;

    void RecvPacket(class clsSession &session);
    void SendPacket(ull SessionID, CClientMessage *msg, BYTE SendType,
                    std::vector<ull> *pIDVector, WORD wVecLen);

    void PostReQuest_iocp(ull SessionID , CClientMessage *msg);
    void Unicast(ull SessionID, CClientMessage *msg, LONG64 Account = 0);

    void RecvComplete(DWORD transferred);
    void SendComplete(DWORD transferred);

    void PostComplete( CClientMessage* msg);


    void ReleaseComplete();
    void ReleaseSession();

    bool SessionLock(ull SessionID);
    void SessionUnLock(ull SessionID);

    void WSASendError(const DWORD LastError);
    void WSARecvError(const DWORD LastError);

    virtual void OnEnterJoinServer(ull SessionID) = 0; // OnConnect
    virtual void OnLeaveServer() = 0;     // OnRelease

    virtual void OnRecv(ull SessionID, CClientMessage *msg) = 0; //< 하나의 패킷 수신 완료 후

  private:
    HANDLE _hIOCP = INVALID_HANDLE_VALUE;

    // PQCS 전용 Pool
    stTlsObjectPool<stPostOverlapped> postPool;
    std::vector<WinThread> _hWorkerThreads;


    ull g_ID = 0; //  [ IDX :17  SEQ : 47 ]

    bool bEnCording = false;
    int headerSize = 0;
    int _bNagle = 0;

    // PQCS 를 할때 session을 재사용하고있기에. LoginPacket말고 다른메세지가 먼저갈 가능성이 존재.
    ull _seqID = 0; 
    protected:
    clsSession session;
};
