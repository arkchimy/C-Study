// CNetworkLib.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//
#include "CNetworkLib.h"

#include <thread>

thread_local stTlsLockInfo tls_LockInfo;
BOOL DomainToIP(const wchar_t *szDomain, IN_ADDR *pAddr)
{
    ADDRINFOW *pAddrInfo;
    SOCKADDR_IN *pSockAddr;
    if (GetAddrInfo(szDomain, L"0", NULL, &pAddrInfo) != 0)
    {
        return FALSE;
    }
    pSockAddr = (SOCKADDR_IN *)pAddrInfo->ai_addr;
    *pAddr = pSockAddr->sin_addr;
    FreeAddrInfo(pAddrInfo);
    return TRUE;
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
    printf("LogicalProcess Cnt : %d \n", temp);

    delete[] infos;
    out = temp;
    return true;
}
//SRWLOCK srw_Log;

void CLanServer::WorkerThread()
{

    clsDeadLockManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo);

    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"This is WorkerThread");
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-20s ",
                                       L"This is WorkerThread");
    }

    // 매개변수를 통한 초기화

    static LONG64 s_arrTPS_idx = 0;
    LPVOID arrTPS_idx = TlsGetValue(m_tlsIdxForTPS);
    // arrTPS 의 index 값을 셋팅함.
    if (arrTPS_idx == nullptr)
    {
        TlsSetValue(m_tlsIdxForTPS, (void *)_interlockedincrement64(&s_arrTPS_idx));
    }

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    ull local_IoCount;
    BOOL bGQCS;
    while (1)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        bGQCS = GetQueuedCompletionStatus(m_hIOCP, &transferred, &key, &overlapped, INFINITE);

        // 종료메세지
        if (transferred == 0 && overlapped == nullptr && key == 0)
            break;

        // overalpped가 nullptr인 메세지를 PQCS 하지 않도록 하기.
        // 만일 진짜 실패면 지역변수를 초기화 하였기에 IocpWorkerThread가 종료.
        if (overlapped == nullptr && bGQCS)
        {
            CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"GetQueuedCompletionStatus Overlapped is nullptr");
            __debugbreak(); // PQCS로 overlapped에 nullptr을 넣음.
        }
        else if (overlapped == nullptr)
        {
            CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"GQCS Failed overlapped is nullptr : %05d", GetLastError());
            break;
        }
        session = reinterpret_cast<clsSession *>(key);
        // TODO : JumpTable 생성 되는 지?
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            //// FIN 의 경우에
            if (transferred == 0)
            {
                // CancleIO 유도
                session->m_blive = false;
            }

            RecvComplete(*session, transferred);
            break;
        case Job_Type::Send:
            SendComplete(*session, transferred);
            break;
        default:
            CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"UnDefine Error Overlapped_mode : %d", reinterpret_cast<stOverlapped *>(overlapped)->_mode);
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
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"WorkerThreadID Terminated ");

}

void CLanServer::AcceptThread()
{

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is AcceptThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is AcceptThread");

    SOCKET client_sock;
    SOCKADDR_IN addr;

    DWORD listen_retval;
    DWORD flag;

    ull session_id = 0;
    ull idx;

    int addrlen;
    stSessionId stsessionID;

    addrlen = sizeof(addr);

    listen_retval = listen(m_listen_sock, SOMAXCONN_HINT(65535));
    flag = 0;

    // InitializeSRWLock(&srw_Log);

    if (listen_retval == 0)
        printf("Listen Sucess\n");
    else
        CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::ERROR_Mode, L"Listen_Falied %d", WSAGetLastError());
    while (1)
    {
        client_sock = accept(m_listen_sock, (sockaddr *)&addr, &addrlen);
        if (client_sock == INVALID_SOCKET)
        {
            CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::ERROR_Mode, L"accept Reseult INVALID_SOCKET  GetLastError : %05d", WSAGetLastError());
            break;
        }

        arrTPS[0]++; // Accept TPS 측정
        m_TotalAccept++;

        {
            // 예상한 Session을 초과한다면 새로 들어온 연결을 끊음.
            if (m_SessionIdxStack.Pop(idx) == false)
            {
                closesocket(client_sock);
                InterlockedIncrement(&iDisCounnectCount);
                continue;
            }
        }

        clsSession &session = sessions_vec[idx];

        if (session.m_sendBuffer.m_size != 0)
            __debugbreak();
        // Session 초기화 부분.
        {
            stsessionID.idx = idx;
            stsessionID.seqNumber = session_id++;

            session.m_sock = client_sock;
            session.m_blive = true;
            session.m_flag = 0;

            InterlockedExchange(&session.m_SeqID.SeqNumberAndIdx, stsessionID.SeqNumberAndIdx);
            InterlockedExchange(&session.m_ioCount, 1); // 1로 시작하므로써 0으로 초기화때 Contents에서 오인하는 일을 방지.
        }

        _interlockedincrement64(&m_SessionCount);
        // CreateIoCompletionPort 함수가 실패하면 반환 값은 NULL입니다.
        {
            HANDLE createResult = CreateIoCompletionPort((HANDLE)client_sock, m_hIOCP, (ull)&session, 0);
            if (createResult == NULL)
            {
                CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::ERROR_Mode,
                                               L"CreateIoCompletionPort Reseult NULL GetLastError : %05d", GetLastError());
                closesocket(client_sock);
                continue;
            }
        }
        // AllocMsg의 처리가 너무 많이 발생한다면 False를 반환.
        if (OnAccept(session.m_SeqID.SeqNumberAndIdx, addr) == false)
        {
            DecrementIoCountAndMaybeDeleteSession(session);
            continue;
        }

        RecvPacket(session);

        DecrementIoCountAndMaybeDeleteSession(session);
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"AcceptThread Terminated %d", 0);

}

CLanServer::CLanServer(bool EnCoding)
    : bEnCording(EnCoding)
{
    hReadyForStopEvent = CreateEvent(nullptr,true, false, nullptr);
    if (hReadyForStopEvent == nullptr)
        __debugbreak();


    if (bEnCording)
        headerSize = sizeof(stHeader);
    else
        headerSize = offsetof(stHeader, sDataLen);

    m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_sock == INVALID_SOCKET)
    {
        CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::ERROR_Mode, L"listen_sock Create Socket Error %d", GetLastError());
        __debugbreak();
    }
}
CLanServer::~CLanServer()
{
    // closesocket(m_listen_sock);
}

BOOL CLanServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions)
{
    linger linger{1,0};
    int buflen;
    DWORD lProcessCnt;
    DWORD bind_retval;
    HRESULT hr;
    SOCKADDR_IN serverAddr;

    m_tlsIdxForTPS = TlsAlloc();
    if (m_tlsIdxForTPS == TLS_OUT_OF_INDEXES)
        __debugbreak();

    sessions_vec.resize(MaxSessions);

    for (ull idx = 0; idx < MaxSessions; idx++)
        m_SessionIdxStack.Push(idx);


    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    InetPtonW(AF_INET, bindAddress, &serverAddr.sin_addr);

    if (ZeroCopy)
    {
        bZeroCopy = true;
        buflen = 0;
        setsockopt(m_listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
        getsockopt(m_listen_sock, SOL_SOCKET, SO_SNDBUF, (char *)&buflen, &buflen);
        printf("getsockopt SendBuffer Len : %d \n", buflen);
    }

    setsockopt(m_listen_sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
    setsockopt(m_listen_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));
    bNoDelay = noDelay;

    bind_retval = bind(m_listen_sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bind_retval != 0)
        CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::ERROR_Mode, L"Bind Failed %d", GetLastError());

    if (GetLogicalProcess(lProcessCnt) == false)
        CSystemLog::GetInstance()->Log(L"GetLogicalProcessError.txt", en_LOG_LEVEL::ERROR_Mode, L"GetLogicalProcess_Error %d", GetLastError());

    m_hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, lProcessCnt - reduceThreadCount);

   
    arrTPS = new LONG64[WorkerCreateCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (WorkerCreateCnt + 1));

    m_WorkThreadCnt = WorkerCreateCnt;


    bOn = true;

    m_hAccept = WinThread(&CLanServer::AcceptThread, this);
    SetThreadDescription(m_hAccept.native_handle(), L"\tAcceptThread");

    m_hWorkerThread.reserve(WorkerCreateCnt);
    for (int i = 0; i < WorkerCreateCnt; i++)
    {
        m_hWorkerThread.emplace_back(&CLanServer::WorkerThread, this);
        std::wstring name = L"\tWorkerThread" + std::to_wstring(i);

        hr = SetThreadDescription(m_hWorkerThread[i].native_handle(), name.c_str());

    }

    return true;
}

void CLanServer::Stop()
{

    closesocket(m_listen_sock);
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"m_listen_sock");
    for (clsSession &session : sessions_vec)
    {
        Disconnect(session.m_SeqID.SeqNumberAndIdx);
    }
    CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"WaitForSingleObject : hReadyForStopEvent");
    //SignalOnForStop 호출을 대기
    WaitForSingleObject(hReadyForStopEvent, INFINITE);

    for (int i = 0; i < m_WorkThreadCnt; i++)
        PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr);

}

void CLanServer::SignalOnForStop()
{
    SetEvent(hReadyForStopEvent);
}

bool CLanServer::Disconnect(const ull SessionID)
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

void CLanServer::CancelIO_Routine(const ull SessionID)
{
    // Session에 대한 안정성은  외부에서 보장해주세요.
    BOOL retval;
    clsSession &session = sessions_vec[SessionID >> 47];

    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);
}

LONG64 CLanServer::GetSessionCount() const
{
    // AcceptThread에서 전담한다면, interlock이 필요없다.
    return m_SessionCount;
}

LONG64 CLanServer::Get_IdxStack() const
{
    return m_SessionIdxStack.m_size;
}

void CLanServer::RecvComplete(clsSession &session, DWORD transferred)
{
    stHeader header;
    ringBufferSize useSize;

    ull SessionID;
    bool bChkSum = false;
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
        {
            bChkSum = msg->DeCoding();
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
            OnRecv(SessionID, msg);
        }
    }
    RecvPacket(session);
}

void CLanServer::DecrementIoCountAndMaybeDeleteSession(clsSession &session)
{
    LONG64 local_IoCount;

    local_IoCount = InterlockedDecrement(&session.m_ioCount);
    if (local_IoCount == 0)
    {

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) != 0)
            return;
        ReleaseSession(session.m_SeqID.SeqNumberAndIdx);
    }
}

CMessage *CLanServer::CreateMessage(clsSession &session, struct stHeader &header) const
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

void CLanServer::SendComplete(clsSession &session, DWORD transferred)
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

        if (bZeroCopy)
        {
            Profiler profile(L"ZeroCopy WSASend");
            send_retval = WSASend(session.m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
            LastError = GetLastError();
        }
        else
        {
            Profiler profile(L"NoZeroCopy WSASend");
            send_retval = WSASend(session.m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
            LastError = GetLastError();
        }
        if (send_retval < 0)
            WSASendError(LastError, session.m_SeqID.SeqNumberAndIdx);
    }
}
bool CLanServer::SessionLock(ull SessionID)
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
void CLanServer::SessionUnLock(ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    // TODO :  ContentsThread에서 Contents_Enq하는 경우의 수. 문제가없는가?
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
}

void CLanServer::SendPacket(ull SessionID, CMessage *msg, BYTE SendType,
                            std::vector<ull> *pIDVector, size_t wVecLen)
{
    //InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]);
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
void CLanServer::Unicast(ull SessionID, CMessage *msg, LONG64 Account)
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

  
        {
            Profiler profile(L"LFQ_Push");
            session.m_sendBuffer.Push(msg);
        }

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
void CLanServer::BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, size_t wVecLen)
{
    ull local_IoCount;

    InterlockedExchange64(&msg->iUseCnt, wVecLen);
    for (size_t i = 0; i < wVecLen; i++)
    {
        ull currentSessionID = (*pIDVector)[i];
        if (SessionLock(currentSessionID) == false)
        {
            stTlsObjectPool<CMessage>::Release(msg);
            continue;
        }
        clsSession &session = sessions_vec[currentSessionID >> 47];

        // 여기까지 왔다면, 같은 Session으로 판단하자.
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
        
        SessionUnLock(currentSessionID);
    }

}
void CLanServer::RecvPacket(clsSession &session)
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
            WSARecvError(LastError, session.m_SeqID.SeqNumberAndIdx);
    }
}

int CLanServer::getAcceptTPS()
{
    return 0;
}
int CLanServer::getRecvMessageTPS()
{
    return 0;
}
int CLanServer::getSendMessageTPS()
{
    return 0;
}

void CLanServer::WSASendError(const DWORD LastError, const ull SessionID)
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

void CLanServer::WSARecvError(const DWORD LastError, const ull SessionID)
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

void CLanServer::ReleaseSession(ull SessionID)
{
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

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ],%10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"closesocketError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
    }
    else
    {
        m_SessionIdxStack.Push(SessionID >> 47);
        _interlockeddecrement64(&m_SessionCount);
    }
}
