#include "CLanClient.h"

// TODO: 라이브러리 함수의 예제입니다.
void fnCLanClient()
{
}

BOOL GetLogicalProcess2(DWORD &out)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *infos = nullptr;
    DWORD len;
    DWORD cnt;
    DWORD temp = 0;

    len = 0;

    GetLogicalProcessorInformation(infos, &len);

    infos = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];

    if (infos == nullptr)
        return false;

    GetLogicalProcessorInformation(infos, &len);

    cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); // 반복문

    for (DWORD i = 0; i < cnt; i++)
    {
        if (infos[i].Relationship == RelationProcessorCore)
        {
            ULONG_PTR mask = infos[i].ProcessorMask;
            // 논리 프로세서의 수는 set된 비트의 개수
            while (mask)
            {
                temp += (mask & 1);
                mask >>= 1;
            }
        }
    }
    delete[] infos;
    out = temp;
    return true;
}

CLanClient::CLanClient(bool EnCoding)
    : bEnCording(EnCoding)
{
    if (bEnCording)
        headerSize = sizeof(stHeader);
    else
        headerSize = offsetof(stHeader, sDataLen);
}

void CLanClient::WorkerThread()
{

    DWORD transferred;
    unsigned long long key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.
    ull local_IoCount;

    while (1)
    {
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        GetQueuedCompletionStatus(_hIOCP, &transferred, &key, &overlapped, INFINITE);
        // transferred 이 0 인 경우에는 상대방이 FIN으로 Send를 닫았는데 WSARecv를 걸경우 0을 반환함.

        if (transferred == 0 && overlapped == nullptr && key == 0)
            break;
        if (overlapped == nullptr)
        {
            // CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"GetQueuedCompletionStatus Overlapped is nullptr");
            continue;
        }
        session = reinterpret_cast<clsSession *>(key);
        // TODO : JumpTable 생성 되는 지?
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            //// FIN 의 경우에
            // if (transferred == 0)
            //     break;
            RecvComplete(transferred);
            break;
        case Job_Type::Send:
            SendComplete(transferred);
            break;
        case Job_Type::ReleasePost:
            ReleaseComplete();
            continue;
        case Job_Type::Post:
            PostComplete(reinterpret_cast<stPostOverlapped *>(overlapped)->msg);
            postPool.Release(overlapped);
            continue;
        default:
            // CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"UnDefine Error Overlapped_mode : %d", reinterpret_cast<stOverlapped *>(overlapped)->_mode);
            __debugbreak();
        }
        local_IoCount = InterlockedDecrement(&session->m_ioCount);

        if (local_IoCount == 0)
        {
            ull compareRetval = InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0);
            if (compareRetval != 0)
            {
                continue;
            }

            ull seqID = session->m_SeqID;
            ReleaseSession();
        }
    }
}

bool CLanClient::Connect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress, int workerThreadCnt, int bNagle, int reduceThreadCount, int userCnt)
{
    DWORD logicalProcess;
    linger linger;
    SOCKADDR_IN serverAddr;
    SOCKET sock;
    _bNagle = bNagle;
    // ConcurrentThread
    {
        GetLogicalProcess2(logicalProcess);
        _hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, logicalProcess - reduceThreadCount);
        // Create WorkerThread

        _hWorkerThreads.resize(workerThreadCnt);
        for (int i = 0; i < workerThreadCnt; i++)
        {
            _hWorkerThreads[i] = WinThread(&CLanClient::WorkerThread, this);
        }
    }

    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Serverport);
    InetPtonW(AF_INET, ServerAddress, &serverAddr.sin_addr);

    linger.l_onoff = 1;
    linger.l_linger = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        __debugbreak();
    }

    setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&_bNagle, sizeof(_bNagle));
    int connectRetval;
    connectRetval = connect(sock, (const sockaddr *)&serverAddr, sizeof(serverAddr));
    if (connectRetval == SOCKET_ERROR)
    {
        DWORD lastError = GetLastError();
        __debugbreak();
    }
    session.m_sock = sock;
    session.m_blive = true;
    CreateIoCompletionPort((HANDLE)session.m_sock, _hIOCP, (ULONG_PTR)&session, 0);
    session.m_ioCount = 1;

    OnEnterJoinServer();

    RecvPacket(session);

    session.m_ioCount--;
    return true;
}
bool CLanClient::ReConnect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress )
{
    DWORD logicalProcess;
    linger linger;
    SOCKADDR_IN serverAddr;
    SOCKET sock;

    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Serverport);
    InetPtonW(AF_INET, ServerAddress, &serverAddr.sin_addr);

    linger.l_onoff = 1;
    linger.l_linger = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        __debugbreak();
    }

    setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&_bNagle, sizeof(_bNagle));
    int connectRetval;
    CSystemLog::GetInstance()->Log(L"CLanClientError", en_LOG_LEVEL::SYSTEM_Mode, L"ReConnect");
retry:

    connectRetval = connect(sock, (const sockaddr *)&serverAddr, sizeof(serverAddr));
    if (connectRetval == SOCKET_ERROR)
    {
        goto retry;
    }
    CSystemLog::GetInstance()->Log(L"CLanClientError", en_LOG_LEVEL::SYSTEM_Mode, L"ReConnect Success");
    session.m_sock = sock;
    session.m_blive = true;
    session.m_ioCount = 1;

    CreateIoCompletionPort((HANDLE)session.m_sock, _hIOCP, (ULONG_PTR)&session, 0);
    OnEnterJoinServer();

    RecvPacket(session);
    session.m_ioCount--;

    return true;

}
void CLanClient::Disconnect()
{
    closesocket(session.m_sock);
    OnLeaveServer();
}

CClientMessage *CLanClient::CreateMessage(clsSession &session, stHeader &header) const
{
    // TODO : Header를 읽고, 생성하고

    CClientMessage *msg;
    ringBufferSize deQsize;

    // 메세지 할당
    {

        {
            Profiler profile(L"PoolAlloc");
            msg = reinterpret_cast<CClientMessage *>(stTlsObjectPool<CClientMessage>::Alloc());
        }
    }
    // 순수하게 데이터만 가져옴.  EnCording 의 경우 RandKey와 CheckSum도 가져옴.
    deQsize = session.m_recvBuffer.Dequeue(msg->_frontPtr, header.sDataLen + headerSize);
    msg->_rearPtr = msg->_frontPtr + deQsize;

    if (header.sDataLen + headerSize != deQsize)
    {
        return nullptr;
    }
    return msg;
}
void CLanClient::RecvPacket(clsSession &session)
{
    ringBufferSize freeSize;
    ringBufferSize directEnQsize;

    int wsaRecv_retval = 0;
    DWORD LastError;
    DWORD flag = 0;
    DWORD bufCnt;
    ull local_IoCount;

    WSABUF localRecvWSABuf[2]{0};

    char *f = session.m_recvBuffer._frontPtr, *r = session.m_recvBuffer._rearPtr;

    {
        ZeroMemory(&session.m_recvOverlapped, sizeof(OVERLAPPED));
    }

    directEnQsize = session.m_recvBuffer.DirectEnqueueSize(f, r);
    freeSize = session.m_recvBuffer.GetFreeSize(f, r); // SendBuffer에 바로넣기 위함.

    if (freeSize == 0)
    {
        // Attack : 조작된 Len으로 인해 리시브 버퍼가 가득참.
        InterlockedExchange(&session.m_blive, 0);
        CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
        return;
    }

    if (freeSize < directEnQsize)
        __debugbreak();
    if (freeSize <= directEnQsize)
    {
        localRecvWSABuf[0].buf = r;
        localRecvWSABuf[0].len = (ULONG)directEnQsize;

        bufCnt = 1;
    }
    else
    {
        localRecvWSABuf[0].buf = r;
        localRecvWSABuf[0].len = (ULONG)directEnQsize;

        localRecvWSABuf[1].buf = session.m_recvBuffer._begin;
        localRecvWSABuf[1].len = (ULONG)(freeSize - directEnQsize);

        bufCnt = 2;
    }

    if (session.m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session.m_ioCount);
        wsaRecv_retval = WSARecv(session.m_sock, localRecvWSABuf, bufCnt, nullptr, &flag, &session.m_recvOverlapped, nullptr);

        LastError = GetLastError();
        if (wsaRecv_retval < 0)
            WSARecvError(LastError);
    }
}
void CLanClient::SendPacket(CClientMessage *msg, BYTE SendType, std::vector<ull> *pIDVector, WORD wVecLen)
{
    Unicast(msg);
}
void CLanClient::PostReQuest_iocp(CClientMessage *msg)
{
    if (SessionLock() == false)
    {
        CSystemLog::GetInstance()->Log(L"CLanClientError", en_LOG_LEVEL::ERROR_Mode, L"PostReQuest_iocp SessionLockFail");
        return;
    }

    stPostOverlapped *overlapped = (stPostOverlapped*)postPool.Alloc();
    overlapped->msg = msg;
    ZeroMemory(overlapped, sizeof(OVERLAPPED));
    
    PostQueuedCompletionStatus(_hIOCP, 0, (ULONG_PTR)&session, overlapped);
    SessionUnLock();

}

void CLanClient::Unicast(CClientMessage *msg, LONG64 Account)
{
    Profiler profile(L"UnitCast_Cnt");

    // 여기까지 왔다면, 같은 Session으로 판단하자.
    ull local_IoCount;

    if (SessionLock() == false)
        return;

    {
        Profiler profile(L"LFQ_Push");
        session.m_sendBuffer.Push(msg);
    }

    // PQCS를 시도.
    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

        local_IoCount = InterlockedIncrement(&session.m_ioCount);

        PostQueuedCompletionStatus(_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
    }
    SessionUnLock();

}

void CLanClient::RecvComplete(DWORD transferred)
{
    stHeader header;
    ringBufferSize useSize;

    ull SessionID;
    bool bChkSum = false;
    {
        session.m_recvBuffer.MoveRear(transferred);
        SessionID = session.m_SeqID;
    }
    // Header의 크기만큼을 확인.
    while (session.m_recvBuffer.Peek(&header, headerSize) == headerSize)
    {
        useSize = session.m_recvBuffer.GetUseSize();
        if (useSize < header.sDataLen + headerSize)
        {
            // 데이터가 덜 옴.
            break;
        }
        // 메세지 생성
        CClientMessage *msg = CreateMessage(session, header);
        if (msg == nullptr)
            break;
        if (bEnCording)
        {
            {
                Profiler profile(L"DeCoding");
                bChkSum = msg->DeCoding();
            }
            if (bChkSum == false)
            {
                // Attack : 조작된 패킷으로 checkSum이 다름.
                InterlockedExchange(&session.m_blive, 0);
                CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L"%-20s ",
                                                   L" false Packet CheckSum Not Equle ");
                }
                stTlsObjectPool<CClientMessage>::Release(msg);
                return;
            }
        }

        InterlockedExchange(&msg->ownerID, SessionID);

        msg->_frontPtr = msg->_frontPtr + headerSize;

        // PayLoad를 읽고 무엇인가 처리하는 Logic이 NetWork에 들어가선 안된다.
        {
            Profiler profile(L"OnRecv");
            OnRecv(msg);
        }
    }
    RecvPacket(session);
}

void CLanClient::SendComplete(DWORD transferred)
{
    ringBufferSize useSize;

    DWORD bufCnt;
    int send_retval = 0;

    WSABUF wsaBuf[500];
    DWORD LastError;
    // LONG64 beforeTPS;

    ull local_IoCount;
    CClientMessage *msg;

    for (DWORD i = 0; i < session.m_sendOverlapped.msgCnt; i++)
    {
        stTlsObjectPool<CClientMessage>::Release(session.m_sendOverlapped.msgs[i]);
    }

    {
        session.m_sendOverlapped.msgCnt = 0;
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
    }

    useSize = (ringBufferSize)session.m_sendBuffer.m_size;

    if (useSize == 0)
    {
        useSize = (ringBufferSize)session.m_sendBuffer.m_size;
        // flag 끄기
        if (_InterlockedCompareExchange(&session.m_flag, 0, 1) == 1)
        {
            useSize = (ringBufferSize)session.m_sendBuffer.m_size;
            if (useSize != 0)
            {
                //// 누군가 진입 했다면 return
                if (_InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
                {
                    ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

                    InterlockedIncrement(&session.m_ioCount);
                    PostQueuedCompletionStatus(_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
                }
            }
        }

        return;
    }

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
    }

    bufCnt = 0;

    {
        Profiler profile(L"LFQ_Pop");
        while (session.m_sendBuffer.Pop(msg))
        {
            wsaBuf[bufCnt].buf = msg->_frontPtr;
            wsaBuf[bufCnt].len = ULONG(msg->_rearPtr - msg->_frontPtr);

            session.m_sendOverlapped.msgs[bufCnt++] = msg;
            if (bufCnt == 500)
            {
                break;
            }
        }
    }

    session.m_sendOverlapped.msgCnt = bufCnt;

    if (session.m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session.m_ioCount);

        Profiler profile(L"NoZeroCopy WSASend");
        send_retval = WSASend(session.m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
        LastError = GetLastError();

        if (send_retval < 0)
            WSASendError(LastError);
    }
}

void CLanClient::PostComplete(CClientMessage* msg) 
{
    // PayLoad를 읽고 무엇인가 처리하는 Logic이 NetWork에 들어가선 안된다.
 
    {
        Profiler profile(L"OnRecv");
        OnRecv(msg);
    }
}
void CLanClient::ReleaseComplete()
{
    // 로직상  Session당 한번만 호출되게 짰음.
    int retval;
    session.Release();
    retval = closesocket(session.m_sock);
    CSystemLog::GetInstance()->Log(L"CLanClientError", en_LOG_LEVEL::SYSTEM_Mode, L"ReleaseComplete");
    OnLeaveServer();

}

void CLanClient::ReleaseSession()
{
    CSystemLog::GetInstance()->Log(L"CLanClientError", en_LOG_LEVEL::SYSTEM_Mode, L"ReleaseSession");

    ZeroMemory(&session.m_releaseOverlapped, sizeof(OVERLAPPED));
    PostQueuedCompletionStatus(_hIOCP, 0, (ULONG_PTR) &session, &session.m_releaseOverlapped);
}
bool CLanClient::SessionLock()
{
    /*
        ※ 인자로 Msg들고오기 싫음.
        => False를 리턴받는다면 호출부에서 msg폐기를 요구합니다.

        * Release된 Session이라면  False를 리턴
        * SessionID가 다르다면     False를 리턴  IO를 감소 시킨후 Release시도 ,  적어도 진입순간에는 Session은 살아있었음.
        * 증가시킨 IO가 '1'인 경우 감소 시킨 후  Release시도 , False를 리턴
        *
    */
    ull Local_ioCount;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        // 새로운 세션으로 초기화되지않았고, r_flag가 세워져있으면 진입하지말자.
        // 이미 r_flag가 올라가있는데 IoCount를 잘못 올린다고 문제가 되지않을 것같다.
        return false;
    }
    if (Local_ioCount == 1)
    {
        // 원래 '0'이 었는데 내가 증가시켰다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession();

        return false;
    }
    return true;
}
void CLanClient::SessionUnLock() 
{
    ull Local_ioCount;

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    // TODO :  ContentsThread에서 Contents_Enq하는 경우의 수. 문제가없는가?
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession();
}
void CLanClient::WSASendError(const DWORD LastError)
{
    ull local_IoCount;
    // TODO : JumpTable이 만들어지는가?
    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (session.m_blive == 0)
        {
            CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
        }
        break;

    case WSAEINTR: // 10004
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;
    case WSAENOTSOCK:     // 10038
    case WSAECONNABORTED: //    10053 :
    case WSAECONNRESET:   // 10054:
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;

    default:
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
}

void CLanClient::WSARecvError(const DWORD LastError)
{
    ull local_IoCount;

    // TODO : JumpTable이 만들어지는가?
    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (session.m_blive == 0)
        {
            CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);
        }
        break;

    case WSAEINTR:        // 10004
    case WSAENOTSOCK:     // 10038
    case WSAECONNABORTED: //    10053 :
    case WSAECONNRESET:   // 10054:
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;

    default:

        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
}
