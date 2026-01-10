#include "CLanClient.h"
BOOL GetLogicalProcess(DWORD &out)
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
        GetQueuedCompletionStatus(m_hIOCP, &transferred, &key, &overlapped, INFINITE);
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
            OnLeaveServer();
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

    // ConcurrentThread
    {
        GetLogicalProcess(logicalProcess);
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, logicalProcess - reduceThreadCount);
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
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&bNagle, sizeof(bNagle));
    int connectRetval;
    connectRetval = connect(sock, (const sockaddr *)&serverAddr, sizeof(serverAddr));
    if (connectRetval == SOCKET_ERROR)
    {
        DWORD lastError = GetLastError();
        __debugbreak();
    }
    session.m_sock = sock;
    CreateIoCompletionPort((HANDLE)session.m_sock, m_hIOCP, (ULONG_PTR)&session, 0);

    return false;
}

void CLanClient::Disconnect()
{
    closesocket(session.m_sock);
}

CMessage *CLanClient::CreateMessage(clsSession &session, stHeader &header) const
{
    // TODO : Header를 읽고, 생성하고

    CMessage *msg;
    ringBufferSize deQsize;

    // 메세지 할당
    {

        {
            Profiler profile(L"PoolAlloc");
            msg = reinterpret_cast<CMessage *>(stTlsObjectPool<CMessage>::Alloc());
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
void CLanClient::SendPacket(CMessage *msg, BYTE SendType, std::vector<ull> *pIDVector, WORD wVecLen)
{
    Unicast(msg);
}
void CLanClient::Unicast(CMessage *msg, LONG64 Account)
{
    Profiler profile(L"UnitCast_Cnt");

    // 여기까지 왔다면, 같은 Session으로 판단하자.
    CMessage **ppMsg;
    ull local_IoCount;
    ppMsg = &msg;

    {
        Profiler profile(L"LFQ_Push");
        session.m_sendBuffer.Push(msg);
    }

    // PQCS를 시도.
    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

        local_IoCount = InterlockedIncrement(&session.m_ioCount);

        PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
    }
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
        CMessage *msg = CreateMessage(session, header);
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
                stTlsObjectPool<CMessage>::Release(msg);
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
    CMessage *msg;

    for (DWORD i = 0; i < session.m_sendOverlapped.msgCnt; i++)
    {
        stTlsObjectPool<CMessage>::Release(session.m_sendOverlapped.msgs[i]);
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
                    PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
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

void CLanClient::ReleaseComplete()
{
    // 로직상  Session당 한번만 호출되게 짰음.
    int retval;

    OnRelease();
    session.Release();
    retval = closesocket(session.m_sock);
}

void CLanClient::ReleaseSession()
{
    ZeroMemory(&session.m_releaseOverlapped, sizeof(OVERLAPPED));

    PostQueuedCompletionStatus(m_hIOCP, 0, session.m_SeqID, &session.m_releaseOverlapped);
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
