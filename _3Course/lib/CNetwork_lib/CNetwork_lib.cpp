#include "CNetwork_lib.h"

#include "../Profiler_MultiThread/Profiler_MultiThread.h"
#include "../../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"

#include "stHeader.h"

#include <list>
#include <thread>

CSystemLog *CLanServer::systemLog = CSystemLog::GetInstance();


BOOL DomainToIP(const wchar_t *szDomain, IN_ADDR *pAddr)
{
    ADDRINFOW *pAddrInfo;
    SOCKADDR_IN *pSockAddr;
    if (GetAddrInfo(szDomain, L"0", NULL, &pAddrInfo) != 0)
    {
        return FALSE;
    }
    pSockAddr = (SOCKADDR_IN *)pAddrInfo->ai_addrlen;
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
SRWLOCK srw_Log;

unsigned AcceptThread(void *arg)
{

    /* arg 에 대해서
     * arg[0] 에는 클라이언트에게 연결시킬 hIOCP의 HANDLE을 넣기.
     * arg[1] 에는 listen_sock의 주소를 넣기
     */
    SOCKET client_sock;
    SOCKADDR_IN addr;

    size_t *arrArg = (size_t *)arg;
    HANDLE hIOCP = (HANDLE)*arrArg;
    SOCKET listen_sock = SOCKET(arrArg[1]);

    DWORD listen_retval;

    DWORD flag;

    ull session_id = 0;
    ull idx;

    clsSession *session;
    CLanServer *server;

    int addrlen;
    stSessionId stsessionID;
    // wchar_t buffer[100];

    addrlen = sizeof(addr);
    server = reinterpret_cast<CLanServer *>(arrArg[2]);

    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
    flag = 0;

    InitializeSRWLock(&srw_Log);

    if (listen_retval == 0)
        printf("Listen Sucess\n");
    else
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Listen_Error");
    while (1)
    {
        client_sock = accept(listen_sock, (sockaddr *)&addr, &addrlen);
        if (client_sock == INVALID_SOCKET)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"Accept Error ");
            continue;
        }

        {

            if (server->m_IdxStack.m_size == 0)
            {
                closesocket(client_sock);
                // 추가적으로 구현해야되는 부분이 존재함.
                //  DisConnect 행위
                __debugbreak();
                continue;
            }
            server->m_IdxStack.Pop(idx);
        }

        session = &server->sessions_vec[idx];
  
        //InterlockedExchange(&session->m_ioCount, 0);
        //InterlockedExchange(&session->m_SeqID.SeqNumberAndIdx, stsessionID.SeqNumberAndIdx);
        //InterlockedExchange(&session->m_sock, client_sock);
        //InterlockedExchange(&session->m_blive, 1);
        //InterlockedExchange(&session->m_flag, 0);

        session->m_ioCount = 0;
        session->m_sock = client_sock;
        session->m_blive = 1;
        session->m_flag = 0;
        session->m_SeqID.SeqNumberAndIdx = (idx << 47 | session_id);
        session_id++;
       
        InterlockedExchange(&session->m_Useflag, 0);



        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu\n",
                                       L"Accept", L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx);

        _interlockedincrement64(&server->m_SessionCount);
        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

        InterlockedIncrement(&session->m_ioCount); // Login의 완료통지가 Recv전에 도착하면 IO_Count가 0이 되버리는 상황 방지.

        server->OnAccept(session->m_SeqID.SeqNumberAndIdx);
        server->RecvPacket(session);

        InterlockedDecrement(&session->m_ioCount);
    }
    return 0;
}

unsigned WorkerThread(void *arg)
{
    /*
     * arg[0] 에는 hIOCP의 가짜핸들을
     * arg[1] 에는 Server의 인스턴스
     */
    size_t *arrArg = reinterpret_cast<size_t *>(arg);
    HANDLE hIOCP = (HANDLE)*arrArg;
    CLanServer *server = reinterpret_cast<CLanServer *>(arrArg[1]);

    static LONG64 s_arrTPS_idx = 0;
    LONG64 *arrTPS_idx = reinterpret_cast<LONG64 *>(TlsGetValue(server->m_tlsIdxForTPS));
    // arrTPS 의 index 값을 셋팅함.
    if (arrTPS_idx == nullptr)
    {
        TlsSetValue(server->m_tlsIdxForTPS, (void *)_interlockedincrement64(&s_arrTPS_idx));
    }

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session;

    ull local_IoCount;
    Profiler profile;


    while (server->bOn)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }

        GetQueuedCompletionStatus(hIOCP, &transferred, &key, &overlapped, INFINITE);

        if (overlapped == nullptr)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"GetQueuedCompletionStatus Overlapped is nullptr");
            continue;
        }
        session = reinterpret_cast<clsSession *>(key);
        // TODO : JumpTable 생성 되는 지?
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            //// FIN 의 경우에
            //if (transferred == 0)
            //    break;
            server->RecvComplete(session, transferred);
            break;
        case Job_Type::Send:
            server->SendComplete(session, transferred);
            break;

        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
            __debugbreak();
        }
        local_IoCount = InterlockedDecrement(&session->m_ioCount);
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"RecvComplete",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
            break;
        case Job_Type::Send:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"SendComplete",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
        }

        if (local_IoCount == 0)
        {
            if (InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0) != 0)
                continue;

            if (InterlockedExchange(&session->m_Useflag, 2) != 0)
                continue;

            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"WorkerRelease",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
            server->ReleaseSession(session->m_SeqID.SeqNumberAndIdx);
        }

    }
    printf("WorkerThreadID : %d  return '0' \n", GetCurrentThreadId());

    return 0;
}
CLanServer::CLanServer()
{

    m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_sock == INVALID_SOCKET)
    {

        ERROR_FILE_LOG(L"Socket_Error.txt",
                       L"listen_sock Create Socket Error");
        __debugbreak();
    }
}
CLanServer::~CLanServer()
{
    closesocket(m_listen_sock);
}

BOOL CLanServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions)
{
    linger linger;
    int buflen;
    DWORD lProcessCnt;
    DWORD bind_retval;

    SOCKADDR_IN serverAddr;

    m_tlsIdxForTPS = TlsAlloc();
    if (m_tlsIdxForTPS == TLS_OUT_OF_INDEXES)
        __debugbreak();

    sessions_vec.resize(MaxSessions);

    for (ull idx = 0; idx < MaxSessions; idx++)
        m_IdxStack.Push(idx);

    linger.l_onoff = 1;
    linger.l_linger = 0;

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

    bind_retval = bind(m_listen_sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bind_retval != 0)
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Bind Failed");

    if (GetLogicalProcess(lProcessCnt) == false)
        ERROR_FILE_LOG(L"GetLogicalProcessError.txt", L"GetLogicalProcess_Error");

    m_hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, lProcessCnt - reduceThreadCount);

    m_hThread = new HANDLE[WorkerCreateCnt];
    arrTPS = new LONG64[WorkerCreateCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (WorkerCreateCnt + 1));

    m_WorkThreadCnt = WorkerCreateCnt;

    {
        WorkerArg[0] = m_hIOCP;
        WorkerArg[1] = this;

        AcceptArg[0] = m_hIOCP;
        AcceptArg[1] = (HANDLE)m_listen_sock;
        AcceptArg[2] = this;
    }

    bOn = true;

    m_hAccept = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &AcceptArg, 0, nullptr);
    for (int i = 0; i < WorkerCreateCnt; i++)
        m_hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &WorkerArg, 0, nullptr);

    return true;
}

void CLanServer::Stop()
{
}

bool CLanServer::Disconnect(const ull SessionID)
{
    //0xffff
    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK ) >> 47];


   
    
    if (InterlockedExchange(&session.m_blive, 0) == 0)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                       L"m_blive is Zero",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Overlapped", &session.m_recvOverlapped);

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"DisconnectFailed_HANDLE value : %lld   seqID :%llu  seqIndx : %llu\n",
                                       session.m_sock, session.m_SeqID.SeqNumberAndIdx, session.m_SeqID.idx);
        return false;
    }
    _interlockedincrement64((LONG64*)&iDisCounnectCount);
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu ",
                                   L"Disconnect",
                                   L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx);

    //취소 요청을 찾을 수 없는 경우 반환 값은 0이고 GetLastError는 ERROR_NOT_FOUND를 반환
    if (CancelIoEx(m_hIOCP, &session.m_recvOverlapped) == 0)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                       L"RecvNotFoundIO",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Overlapped", &session.m_recvOverlapped);
    }
    else
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                       L"RecvCancel",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Overlapped", &session.m_sendOverlapped);
    }
    if (CancelIoEx(m_hIOCP, &session.m_sendOverlapped) == 0)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                       L"SendNotFoundIO",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Overlapped", &session.m_sendOverlapped);
    }
    else
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                       L"SendCancel",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Overlapped", &session.m_sendOverlapped);
    }
  
    return true;
}

int CLanServer::GetSessionCount()
{
    // AcceptThread에서 전담한다면, interlock이 필요없다.
    return m_SessionCount;
}

LONG64 CLanServer::GetReleaseSessions()
{
    return m_ReleaseSessions.m_size;
}

LONG64 CLanServer::Get_IdxStack()
{
    return m_IdxStack.m_size;
}

void CLanServer::RecvComplete(clsSession *const session, DWORD transferred)
{
    session->m_recvBuffer.MoveRear(transferred);

    stHeader header;
    ringBufferSize useSize;
    double qPersentage;

    char *f, *r;
    f = session->m_recvBuffer._frontPtr;
    r = session->m_recvBuffer._rearPtr;

    // ContentsQ의 상황이 어떤지를 체크.
    qPersentage = OnRecv((ull)(session->m_SeqID.SeqNumberAndIdx), nullptr);
    if (qPersentage >= 75.f)
    {
        // RecvPostMessage(session);
        ERROR_FILE_LOG(L"ContentsQ_Full.txt", L"ContentsQ_Full");
        Disconnect(session->m_SeqID.SeqNumberAndIdx);
        return;
    }
    // Header의 크기만큼을 확인.
    while (session->m_recvBuffer.Peek(&header, sizeof(stHeader), f, r) == sizeof(stHeader))
    {
        useSize = session->m_recvBuffer.GetUseSize(f, r);
        if (useSize < header._len + sizeof(stHeader))
        {
            ERROR_FILE_LOG(L"ContentsQ_Full.txt", L"ContentsQ_Full");
            Disconnect(session->m_SeqID.SeqNumberAndIdx);
            return;
        }
        // 메세지 생성
        CMessage *msg = CreateMessage(session, header);
        {
            Profiler profile;
            profile.Start(L"OnRecv");
            qPersentage = OnRecv(session->m_SeqID.SeqNumberAndIdx, msg);
            profile.End(L"OnRecv");
        }
        f = session->m_recvBuffer._frontPtr;
        if (qPersentage >= 75.0)
        {
            // RecvPostMessage(session);
            ERROR_FILE_LOG(L"ContentsQ_Full.txt", L"ContentsQ_Full");
            Disconnect(session->m_SeqID.SeqNumberAndIdx);
            return;
        }
    }

    if (session->m_blive)
        RecvPacket(session);
}

CMessage *CLanServer::CreateMessage(clsSession *const session, class stHeader &header)
{
    unsigned short type = 0;
    Profiler profile;

    profile.Start(L"PoolAlloc");
    CMessage *msg = reinterpret_cast<CMessage *>(CMessagePoolManager::pool.Alloc());
    profile.End(L"PoolAlloc");

  

    ringBufferSize deQsize;

    switch (type)
    {
    case 0:
    default:
    {
        deQsize = session->m_recvBuffer.Dequeue(msg->_begin, sizeof(header) + header._len);
        msg->_rearPtr = msg->_begin + deQsize;
        msg->_begin = msg->_begin;
    }
    }
    return msg;
}
char *CLanServer::CreateLoginMessage()
{
    static short header = 0x0008;
    static ull PayLoad = 0x7fffffffffffffff;
    static char msg[10];

    memcpy(msg, &header, sizeof(header));
    memcpy(msg + sizeof(header), &PayLoad, sizeof(PayLoad));

    return msg;
}


void CLanServer::SendComplete(clsSession *const session, DWORD transferred)
{
    size_t m_SendMsgSize = session->m_SendMsg.size();

    for (CMessage *msg : session->m_SendMsg)
    {
        CMessagePoolManager::pool.Release(msg);
    }
    session->m_SendMsg.clear();
    SendPacket(session);
}

void CLanServer::SendPacket(clsSession *const session)
{

    ringBufferSize useSize, directDQSize;
    ringBufferSize deQSize;

    DWORD bufCnt;
    int send_retval;

    WSABUF wsaBuf[500];
    DWORD LastError;
    // LONG64 beforeTPS;
    LONG64 TPSidx;
    ull local_IoCount;

    useSize = session->m_sendBuffer.m_size;
    TPSidx = (LONG64)TlsGetValue(m_tlsIdxForTPS);

    if (useSize == 0)
    {
        // flag 끄기
        if (_InterlockedCompareExchange(&session->m_flag, 0, 1) == 1)
        {
            useSize = session->m_sendBuffer.m_size;
            if (useSize != 0)
            {
                // 누군가 진입 했다면 return
                if (_InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
                {
                    ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
                    InterlockedIncrement(&session->m_ioCount);
                    PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_sendOverlapped);
                }
            }
        }
        return;
    }

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
    }

    ull msgAddr;
    CMessage *msg;

    bufCnt = 0;

    {
        Profiler profile;
        profile.Start(L"LFQ_Pop");
        while (session->m_sendBuffer.Pop(msg))
        {

            session->m_SendMsg.push_back(msg);

            wsaBuf[bufCnt].buf = msg->_frontPtr;
            wsaBuf[bufCnt].len = SerializeBufferSize(msg->_rearPtr - msg->_frontPtr);
            bufCnt++;
            useSize -= 8;
        }
        profile.End(L"LFQ_Pop");
    }
    local_IoCount = _InterlockedIncrement(&session->m_ioCount);

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"WSASend",
                                   L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                   L"IO_Count", local_IoCount);
    arrTPS[TPSidx] += bufCnt;

    if (bZeroCopy)
    {
        Profiler::Start(L"ZeroCopy WSASend");
        send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
        LastError = GetLastError();
        Profiler::End(L"ZeroCopy WSASend");
    }
    else
    {
        Profiler::Start(L"WSASend");
        send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
        LastError = GetLastError();
        Profiler::End(L"WSASend");
    }

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"WSASend",
                                   L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                   L"IO_Count", local_IoCount);
    if (send_retval < 0)
        WSASendError(LastError, session->m_SeqID.SeqNumberAndIdx);

}

void CLanServer::RecvPacket(clsSession *const session)
{
    ringBufferSize freeSize;
    ringBufferSize directEnQsize;

    int wsaRecv_retval;
    DWORD LastError;
    DWORD flag = 0;
    DWORD bufCnt;

    char *f = session->m_recvBuffer._frontPtr, *r = session->m_recvBuffer._rearPtr;

    {
        ZeroMemory(session->m_lastRecvWSABuf, sizeof(session->m_lastRecvWSABuf));
        ZeroMemory(&session->m_recvOverlapped, sizeof(OVERLAPPED));
    }

    directEnQsize = session->m_recvBuffer.DirectEnqueueSize(f, r);
    freeSize = session->m_recvBuffer.GetFreeSize(f, r); // SendBuffer에 바로넣기 위함.
    if (freeSize < directEnQsize)
        __debugbreak();
    if (freeSize <= directEnQsize)
    {
        session->m_lastRecvWSABuf[0].buf = r;
        session->m_lastRecvWSABuf[0].len = directEnQsize;

        bufCnt = 1;
    }
    else
    {
        session->m_lastRecvWSABuf[0].buf = r;
        session->m_lastRecvWSABuf[0].len = directEnQsize;

        session->m_lastRecvWSABuf[1].buf = session->m_recvBuffer._begin;
        session->m_lastRecvWSABuf[1].len = freeSize - directEnQsize;

        bufCnt = 2;
    }

    ull local_IoCount = _InterlockedIncrement(&session->m_ioCount);

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"WSARecv",
                                   L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                   L"IO_Count", local_IoCount);

    wsaRecv_retval = WSARecv(session->m_sock, session->m_lastRecvWSABuf, bufCnt, nullptr, &flag, &session->m_recvOverlapped, nullptr);
    if (wsaRecv_retval < 0)
    {
        LastError = GetLastError();
        WSARecvError(LastError, session->m_SeqID.SeqNumberAndIdx);
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

void CLanServer::WSASendError(const DWORD LastError,const ull SessionID)
{
    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    ull local_IoCount;

    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (session.m_blive == 0)
        {
            // 함수가 성공하면 반환 값이 0이 아닙니다.
            if (CancelIoEx(m_hIOCP, &session.m_sendOverlapped) != 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                               L"m_blive is Zero",
                                               L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"Overlapped", &session.m_recvOverlapped);
            }
            // 함수가 성공하면 반환 값이 0이 아닙니다.
            if (CancelIoEx(m_hIOCP, &session.m_recvOverlapped) != 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                               L"m_blive is Zero",
                                               L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"Overlapped", &session.m_recvOverlapped);
            }
        }
        break;

        // WSAENOTSOCK
    case 10038:
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                       L"WSASendError",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"GetLastError", LastError);
        break;
        // WSAECONNABORTED 내가 끊음.
    case 10053:
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                       L"ServerRST",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"GetLastError", LastError);
        if (InterlockedExchange(&session.m_blive, 0) == 1)
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive => 0",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        else
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive Failed",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        break;

        // WSAECONNRESET  상대가 끊음.
    case 10054:
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                       L"ClientRST",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"GetLastError", LastError);
        if (InterlockedExchange(&session.m_blive, 0) == 1)
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive => 0",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        else
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive Failed",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }

        break;

    default:

        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"UnDefineError",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        if (InterlockedExchange(&session.m_blive, 0) == 1)
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive => 0",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        else
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive Failed",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
    }
}

void CLanServer::WSARecvError(const DWORD LastError, const ull SessionID)
{
    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    ull local_IoCount;

    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (session.m_blive == 0)
        {
            // 함수가 성공하면 반환 값이 0이 아닙니다.
            if (CancelIoEx(m_hIOCP, &session.m_sendOverlapped) != 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                               L"m_blive is Zero",
                                               L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"Overlapped", &session.m_recvOverlapped);
            }
            // 함수가 성공하면 반환 값이 0이 아닙니다.
            if (CancelIoEx(m_hIOCP, &session.m_recvOverlapped) != 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %p",
                                               L"m_blive is Zero",
                                               L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"Overlapped", &session.m_recvOverlapped);
            }
        }
        break;

        // WSAENOTSOCK
    case 10038:
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                       L"WSARecvError",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"GetLastError", LastError);
        break;
        // WSAECONNABORTED 내가 끊음.
    case 10053:
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                       L"ServerRST",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"GetLastError", LastError);
        if (InterlockedExchange(&session.m_blive, 0) == 1)
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                           L"m_blive => 0",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        else
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive Failed",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        break;

        // WSAECONNRESET  상대가 끊음.
    case 10054:
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %5d",
                                       L"ClientRST",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"GetLastError", LastError);
        if (InterlockedExchange(&session.m_blive, 0) == 1)
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive => 0",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        else
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive Failed",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }

        break;

    default:

        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"UnDefineError",
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        if (InterlockedExchange(&session.m_blive, 0) == 1)
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive => 0",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
        else
        {
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3d",
                                           L"m_blive Failed",
                                           L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                           L"m_blive", 0);
        }
    }
}

void CLanServer::ReleaseSession(ull SessionID)
{
    clsSession* session = &sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    if (session->m_SeqID.SeqNumberAndIdx != SessionID)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %012llu",
                                       L"SeqNum!=SessionID",
                                       L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                       L"SessionID", SessionID);
        return;
    }
    session->Release();
    m_ReleaseSessions.Push(session);

    clsSession *releaseSession;
    ull idx;
    while (m_ReleaseSessions.Pop(releaseSession) == true)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Closesocket",
                                       L"HANDLE : ", releaseSession->m_sock, L"seqID :", releaseSession->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", releaseSession->m_SeqID.idx,
                                       L"IO_Count", releaseSession->m_ioCount);
        closesocket(releaseSession->m_sock);
        //releaseSession->m_sock = 0;
        idx = (releaseSession->m_SeqID.SeqNumberAndIdx & SESSION_IDX_MASK) >> 47;
        //releaseSession->m_SeqID.SeqNumberAndIdx = 0;
        m_IdxStack.Push(idx);
        _interlockeddecrement64(&m_SessionCount);
    }
}
