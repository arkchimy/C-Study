#include "CLanClient.h"

void example()
{
    //사용법
    class CTestClient : public CLanClient
    {
        virtual void OnEnterJoinServer(ull SessionID) override {}; //	< 서버와의 연결 성공 후
        virtual void OnLeaveServer(ull SessionID) override {};     //< 서버와의 연결이 끊어졌을 때

        virtual float OnRecv(ull SessionID, CMessage *msg) override { return 0; } //< 하나의 패킷 수신 완료 후
        virtual void OnSend(int sendsize) override {}; //	< 패킷 송신 완료 후
        virtual void OnRelease(ull SessionID) override {}; 
    };
    CTestClient client;
    client.Connect(L"127.0.0.1", 6000, nullptr, 1, 1, 1, 1);

}
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

CLanClient::CLanClient(bool bEnCording)
{

    m_tlsIdxForTPS = TlsAlloc();
    if (m_tlsIdxForTPS == TLS_OUT_OF_INDEXES)
        __debugbreak();

    
    if (bEnCording)
        headerSize = sizeof(stHeader);
    else
        headerSize = offsetof(stHeader, sDataLen);


}

CMessage *CLanClient::CreateMessage(clsSession &session, stHeader &header)
{
    // TODO : Header를 읽고, 생성하고

    CMessage *msg;
    ringBufferSize deQsize;

    // 메세지 할당
    {
        Profiler profile(L"PoolAlloc");
        msg = reinterpret_cast<CMessage *>(stTlsObjectPool<CMessage>::Alloc());
    }
    // 순수하게 데이터만 가져옴.  EnCording 의 경우 RandKey와 CheckSum도 가져옴.
    deQsize = session.m_recvBuffer.Dequeue(msg->_frontPtr, header.sDataLen + headerSize);
    msg->_rearPtr = msg->_frontPtr + deQsize;

    if (header.sDataLen + headerSize != deQsize)
    {
        // TODO : 조작된 패킷
        CSystemLog::GetInstance()->Log(L"Disconnect", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %10s %05d %10s %05d",
                                       L"riggedPacket",
                                       L"DequeueSize : ", sizeof(stHeader),
                                       L"Retval : ", deQsize);
        msg->HexLog();
        return nullptr;
    }
    return msg;
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
            RecvComplete(*session, transferred);
            break;
        case Job_Type::Send:
            SendComplete(*session, transferred);
            break;
        case Job_Type::ReleasePost:
            ReleaseComplete(session->m_SeqID.SeqNumberAndIdx);
            OnLeaveServer(session->m_SeqID.SeqNumberAndIdx);
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

            ull seqID = session->m_SeqID.SeqNumberAndIdx;
            ReleaseSession(seqID);
        }
    }
}

bool CLanClient::Connect(const wchar_t *ServerAddress, short Serverport,const wchar_t *BindipAddress, int workerThreadCnt, int bNagle, int reduceThreadCount, int userCnt)
{
    DWORD logicalProcess;
    linger linger;
    SOCKADDR_IN serverAddr;

    arrTPS = new LONG64[workerThreadCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (workerThreadCnt + 1));

    // ConcurrentThread
    {
        GetLogicalProcess(logicalProcess);
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, logicalProcess - reduceThreadCount);
        // Create WorkerThread
        {
            _hIocpThreadVec.resize(workerThreadCnt);
            for (int i = 0; i < workerThreadCnt; i++)
            {
                _hIocpThreadVec[i] = std::thread(&CLanClient::WorkerThread, this);
            }
        }
    }

    _sockVec.resize(userCnt);
    for (ull i = 0; i < SessionMax; i++)
        _IdxStack.Push(i);

    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Serverport);
    InetPtonW(AF_INET, ServerAddress, &serverAddr.sin_addr);

    linger.l_onoff = 1;
    linger.l_linger = 0;

    for (int i = 0; i < userCnt; i++)
    {
        _sockVec[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockVec[i] == INVALID_SOCKET)
        {

            __debugbreak();
        }

        setsockopt(_sockVec[i], SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
        setsockopt(_sockVec[i], IPPROTO_TCP, TCP_NODELAY, (const char *)&bNagle, sizeof(bNagle));
        int connectRetval;
        connectRetval = connect(_sockVec[i], (const sockaddr *)&serverAddr, sizeof(serverAddr));
        if (connectRetval == SOCKET_ERROR)
        {
            DWORD lastError = GetLastError();
            __debugbreak();
        }
        ull top;
        ull SessionID;
        _IdxStack.Pop(top);

        SessionID = top;

        SessionID = SessionID << 47;
        SessionID += g_ID++;

        sessions_vec[top].m_SeqID.SeqNumberAndIdx = SessionID;
        sessions_vec[top].m_sock = _sockVec[i];
        OnEnterJoinServer(SessionID);

        CreateIoCompletionPort((HANDLE)sessions_vec[top].m_sock, m_hIOCP, (ULONG_PTR)&sessions_vec[top], 0);
    }

    return false;
}

bool CLanClient::Disconnect(const ull SessionID)
{
    // WorkerThread에서 호출하는 DisConnect이므로  IO가 '0' 이 되는 경우의 수가 없음.
    clsSession &session = sessions_vec[SessionID >> 47];

    ull Local_ioCount;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        return false;
    }

    if (SessionID != session.m_SeqID.SeqNumberAndIdx)
    {

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %018llu  %10s %4llu %10s %018llu  %10s %4llu ",
                                       L"Disconnect",
                                       L"HANDLE : ", session.m_sock,
                                       L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"LocalseqID :", SessionID, L"LocalseqIndx : ", SessionID >> 47);
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);

        return false;
    }

    ull retval = InterlockedExchange(&session.m_blive, 0);
    if (retval == 1)
    {
        InterlockedIncrement(&iDisCounnectCount);
    }
    CancelIO_Routine(SessionID);
    // ContentsThread가 호출하게되면, 연결이 끊긴 Session의 ID가 올 수 있다.

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
    return true;
}

void CLanClient::CancelIO_Routine(const ull SessionID)
{ // Session에 대한 안정성은  외부에서 보장해주세요.
    BOOL retval;
    clsSession &session = sessions_vec[SessionID >> 47];

    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);
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
    if (freeSize < directEnQsize)
        __debugbreak();
    if (freeSize <= directEnQsize)
    {
        localRecvWSABuf[0].buf = r;
        localRecvWSABuf[0].len = directEnQsize;

        bufCnt = 1;
    }
    else
    {
        localRecvWSABuf[0].buf = r;
        localRecvWSABuf[0].len = directEnQsize;

        localRecvWSABuf[1].buf = session.m_recvBuffer._begin;
        localRecvWSABuf[1].len = freeSize - directEnQsize;

        bufCnt = 2;
    }

    if (session.m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session.m_ioCount);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"WSARecv",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        wsaRecv_retval = WSARecv(session.m_sock, localRecvWSABuf, bufCnt, nullptr, &flag, &session.m_recvOverlapped, nullptr);

        LastError = GetLastError();
        if (wsaRecv_retval < 0)
            WSARecvError(LastError, session.m_SeqID.SeqNumberAndIdx);
    }
}

void CLanClient::SendPacket(ull SessionID, CMessage *msg, BYTE SendType, std::vector<ull> *pIDVector, WORD wVecLen)
{
    // InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]);
    switch (SendType)
    {
    case 0:
        Unicast(SessionID, msg);
        break;
    case 1:
        BroadCast(SessionID, msg, pIDVector, wVecLen);
        break;
    }
}
void CLanClient::Unicast(ull SessionID, CMessage *msg, LONG64 Account)
{
    Profiler profile(L"UnitCast_Cnt");
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    clsSession &session = sessions_vec[SessionID >> 47];

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

    SessionUnLock(SessionID);
}
void CLanClient::BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, WORD wVecLen)
{
    InterlockedExchange64(&msg->iUseCnt, wVecLen);
    for (WORD i = 0; i < wVecLen; i++)
    {
        ull currentSessionID = (*pIDVector)[i];
        if (SessionLock(currentSessionID) == false)
        {
            stTlsObjectPool<CMessage>::Release(msg);
            continue;
        }
        clsSession &session = sessions_vec[currentSessionID >> 47];

        // 여기까지 왔다면, 같은 Session으로 판단하자.
        CMessage **ppMsg;
        ull local_IoCount;
        ppMsg = &msg;

        {
            Profiler profile(L"LFQ_Push");
            session.m_sendBuffer.Push(*ppMsg);
        }

        // PQCS를 시도.
        if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
        {
            ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

            local_IoCount = InterlockedIncrement(&session.m_ioCount);

            PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
        }

        SessionUnLock(currentSessionID);
    }
}

void CLanClient::RecvComplete(clsSession &session, DWORD transferred)
{

        stHeader header;
        ringBufferSize useSize;
        float qPersentage;
        ull SessionID;

        {
            session.m_recvBuffer.MoveRear(transferred);
            SessionID = session.m_SeqID.SeqNumberAndIdx;
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
                msg->DeCoding();

            InterlockedExchange(&msg->ownerID, SessionID);

            msg->_frontPtr = msg->_frontPtr + headerSize;
            // PayLoad를 읽고 무엇인가 처리하는 Logic이 NetWork에 들어가선 안된다.

    
            Profiler profile(L"OnRecv");
            qPersentage = OnRecv(SessionID, msg);

            if (qPersentage >= 75.0)
            {
                Disconnect(SessionID);
                CSystemLog::GetInstance()->Log(L"Disconnect", en_LOG_LEVEL::ERROR_Mode,
                                               L"%-20s %10s %05f %10s %08llu",
                                               L"ContentsQ Full",
                                               L"qPersentage : ", qPersentage,
                                               L"TargetID : ", SessionID);
                return;
            }
        }
        RecvPacket(session);
    
}

void CLanClient::SendComplete(clsSession &session, DWORD transferred)
{
    ringBufferSize useSize;

    DWORD bufCnt;
    int send_retval = 0;

    WSABUF wsaBuf[500];
    DWORD LastError;
    // LONG64 beforeTPS;
    LONG64 TPSidx;
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
    TPSidx = (LONG64)TlsGetValue(m_tlsIdxForTPS);

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
    arrTPS[TPSidx] += bufCnt;

    if (session.m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session.m_ioCount);

   
        Profiler profile(L"WSASend");
        send_retval = WSASend(session.m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
        LastError = GetLastError();

        if (send_retval < 0)
            WSASendError(LastError, session.m_SeqID.SeqNumberAndIdx);
    }
}

void CLanClient::ReleaseComplete(ull SessionID)
{
    // 로직상  Session당 한번만 호출되게 짰음.
    int retval;
    clsSession &session = sessions_vec[SessionID >> 47];
    if (SessionID != session.m_SeqID.SeqNumberAndIdx)
    {

        __debugbreak();
        return;
    }

    OnRelease(SessionID);
    session.Release();
    retval = closesocket(session.m_sock);
    DWORD LastError = GetLastError();
    if (retval != 0)
    {
    }
    else
    {
        _IdxStack.Push(SessionID >> 47);
        //_interlockeddecrement64(&m_SessionCount);
    }
}



void CLanClient::ReleaseSession(ull SessionID)
{

    clsSession* session = &sessions_vec[SessionID >> 47];
    ZeroMemory(&session->m_releaseOverlapped, sizeof(OVERLAPPED));

    PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_releaseOverlapped);
}

void CLanClient::WSASendError(const DWORD LastError, const ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
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

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ],%10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Send_UnDefineError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
}

void CLanClient::WSARecvError(const DWORD LastError, const ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
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
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ] %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Recv_UnDefineError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
}

bool CLanClient::SessionLock(ull SessionID)
{
    /*
    ※ 인자로 Msg들고오기 싫음.
    => False를 리턴받는다면 호출부에서 msg폐기를 요구합니다.

    * Release된 Session이라면  False를 리턴
    * SessionID가 다르다면     False를 리턴  IO를 감소 시킨후 Release시도 ,  적어도 진입순간에는 Session은 살아있었음.
    * 증가시킨 IO가 '1'인 경우 감소 시킨 후  Release시도 , False를 리턴
    *
*/
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;
    ull seqID;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        // 새로운 세션으로 초기화되지않았고, r_flag가 세워져있으면 진입하지말자.
        // 이미 r_flag가 올라가있는데 IoCount를 잘못 올린다고 문제가 되지않을 것같다.
        return false;
    }

    // session의 Release는 막았으므로 변경되지않음.
    seqID = session.m_SeqID.SeqNumberAndIdx;
    if (seqID != SessionID)
    {
        // 새로 세팅된 Session이므로 다른 연결이라 판단.
        // 내가 잘못 올린 ioCount를 감소 시켜주어야한다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(seqID);

        return false;
    }

    if (Local_ioCount == 1)
    {
        // 원래 '0'이 었는데 내가 증가시켰다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(SessionID);

        return false;
    }
    return true;
}

void CLanClient::SessionUnLock(ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    // TODO :  ContentsThread에서 Contents_Enq하는 경우의 수. 문제가없는가?
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
}

void clsSession::Release()
{
    CMessage *msg;
    while (m_sendBuffer.Pop(msg) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
    }

    {
        ZeroMemory(&m_recvOverlapped, sizeof(OVERLAPPED));
        ZeroMemory(&m_sendOverlapped, sizeof(OVERLAPPED));
    }
    m_recvBuffer.ClearBuffer();
}
