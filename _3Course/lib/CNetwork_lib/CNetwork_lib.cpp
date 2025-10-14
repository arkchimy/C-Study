#include "CNetwork_lib.h"

#include "../Profiler_MultiThread/Profiler_MultiThread.h"
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
            idx = server->m_IdxStack.Pop();
        }

        stsessionID.idx = idx;
        stsessionID.seqNumber = session_id++;

        session = &server->sessions_vec[idx];
        session->m_SeqID.SeqNumberAndIdx = stsessionID.SeqNumberAndIdx;
        session->m_sock = client_sock;
        session->m_blive = true;

        session->m_Useflag = 0;
        session->m_flag = 0;
        session->m_ioCount = 0;


        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                    L"Accept _ HANDLE value : %lld is not socket  seqID :%llu  seqIndx : %llu\n",
                                    session->m_sock, session->m_SeqID.SeqNumberAndIdx, session->m_SeqID.idx);

        _interlockedincrement64(&server->m_SessionCount);

        //{
        //    wchar_t buffer[1000];
        //    StringCchPrintfW(buffer, sizeof(buffer) / sizeof(wchar_t), L"Accept idx : %llu sessionPrt : %p sock : %llu   ", stsessionID.idx , session, session->m_sock);
        //    ERROR_FILE_LOG(L"SystemLog.txt", buffer);
        //}

        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

        InterlockedIncrement(&session->m_ioCount); // Login의 완료통지가 Recv전에 도착하면 IO_Count가 0이 되버리는 상황 방지.

        server->OnAccept(session->m_SeqID.SeqNumberAndIdx);
        server->RecvPacket(session);

        InterlockedDecrement(&session->m_ioCount);

        /*
        {
            wchar_t buffer[1000];
            StringCchPrintfW(buffer, sizeof(buffer) / sizeof(wchar_t), L"CloseSock idx : %llu  sessionPrt : %p sock : %llu  ", element.m_SeqID.idx, session, session->m_sock);
            ERROR_FILE_LOG(L"SystemLog.txt", buffer);
        }*/
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

    ull local_ioCount;
    Profiler profile;

    bool toggle = FALSE; // GQCS를 획득하고 해제하는 시간을 체크하기위한 방식.

    while (server->bOn)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        if (toggle != TRUE)
            toggle = TRUE;
        else
            profile.End(L"GetQueuedCompletionStatus");

        GetQueuedCompletionStatus(hIOCP, &transferred, &key, &overlapped, INFINITE);

        profile.Start(L"GetQueuedCompletionStatus");
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
        local_ioCount = InterlockedDecrement(&session->m_ioCount);

        if (local_ioCount == 0)
        {
            if (InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0) != 0)
                return 0;

            if (InterlockedExchange(&session->m_Useflag, 2) != 0)
                return 0;
            session->Release();
            server->m_ReleaseSessions.Push(session);
        }
        clsSession *releaseSession;
        while (server->m_ReleaseSessions.Pop(releaseSession))
        {
            closesocket(releaseSession->m_sock);
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"closesocket _ HANDLE value : %lld is not socket  seqID :%llu  seqIndx : %llu\n",
                                           releaseSession->m_sock, releaseSession->m_SeqID.SeqNumberAndIdx, releaseSession->m_SeqID.idx);

            server->m_IdxStack.Push(releaseSession->m_SeqID.idx);
            _interlockeddecrement64(&server->m_SessionCount);
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
    clsSession &session = sessions_vec[SessionID & 0xffff];

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"Disconnect _ HANDLE value : %lld is not socket  seqID :%llu  seqIndx : %llu\n",
                                   session.m_sock, session.m_SeqID.SeqNumberAndIdx, session.m_SeqID.idx);

    if (InterlockedExchange(&session.m_blive, 0) == 0)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"DisconnectFailed_HANDLE value : %lld is not socket  seqID :%llu  seqIndx : %llu\n",
                                       session.m_sock, session.m_SeqID.SeqNumberAndIdx, session.m_SeqID.idx);
        return false;
    }

    CancelIoEx(m_hIOCP, &session.m_recvOverlapped);
    CancelIoEx(m_hIOCP, &session.m_sendOverlapped);

    InterlockedIncrement(&m_DisConnectCount);
    return true;
}

int CLanServer::GetSessionCount()
{
    // AcceptThread에서 전담한다면, interlock이 필요없다.
    return m_SessionCount;
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
        CMessage *msg = CreateCMessage(session, header);
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

CMessage *CLanServer::CreateCMessage(clsSession *const session, class stHeader &header)
{
    Profiler profile;

    profile.Start(L"PoolAlloc");
    CMessage *msg = reinterpret_cast<CMessage *>(CObjectPoolManager::pool.Alloc());
    profile.End(L"PoolAlloc");

    unsigned short type = 0;
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
// CMessage *CLanServer::CreateLoginMessage()
//{
//     static short header = 0x0008;
//     static ull PayLoad = 0x7fffffffffffffff;
//
//     // 현재 이 기능 사용안함.
//     CMessage *msg = nullptr;
//     //static char LoginPacket[10] = {0x08, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
//     //CMessage *msg = reinterpret_cast<CMessage *>(CObjectPoolManager::pool.Alloc());
//     //msg->PutData(LoginPacket, sizeof(LoginPacket));
//
//     return msg;
// }

void CLanServer::SendComplete(clsSession *const session, DWORD transferred)
{
    size_t m_SendMsgSize = session->m_SendMsg.size();

    for (CMessage *msg : session->m_SendMsg)
    {
        CObjectPoolManager::pool.Release(msg);
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

    useSize = session->m_sendBuffer.GetUseSize();

    TPSidx = (LONG64)TlsGetValue(m_tlsIdxForTPS);

    if (useSize == 0)
    {
        // flag 끄기
        if (_InterlockedCompareExchange(&session->m_flag, 0, 1) == 1)
        {
            useSize = session->m_sendBuffer.GetUseSize();
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
    directDQSize = session->m_sendBuffer.DirectDequeueSize();

    ull msgAddr;
    CMessage *msg;

    bufCnt = 0;

    while (useSize != 0)
    {

        //{
        //    디버깅용
        //    deQSize = session->m_sendBuffer.Peek(&msgAddr, sizeof(size_t));
        //    msg = reinterpret_cast<CMessage *>(msgAddr);
        //    wsaBuf[bufCnt].buf = msg->_frontPtr;
        //    wsaBuf[bufCnt].len = SerializeBufferSize(msg->_rearPtr - msg->_frontPtr);
        //}
        deQSize = session->m_sendBuffer.Dequeue(&msgAddr, sizeof(size_t));
        if (deQSize != sizeof(size_t))
            __debugbreak();

        msg = reinterpret_cast<CMessage *>(msgAddr);
        session->m_SendMsg.push_back(msg);

        wsaBuf[bufCnt].buf = msg->_frontPtr;
        wsaBuf[bufCnt].len = SerializeBufferSize(msg->_rearPtr - msg->_frontPtr);
        bufCnt++;
        useSize -= 8;
    }
    _InterlockedIncrement(&session->m_ioCount);

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
    wchar_t buffer[1000];
    if (send_retval < 0)
    {
        switch (LastError)
        {
        case WSA_IO_PENDING:
            if (session->m_blive == 0)
            {
                // 함수가 성공하면 반환 값이 0이 아닙니다.
                if (CancelIoEx(m_hIOCP, &session->m_sendOverlapped) == true)
                {
                    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                                   L"WSASend CancelIoEx HANDLE value : %lld  seqID :%llu  seqIndx : %llu\n",
                                                   session->m_sock, session->m_SeqID.SeqNumberAndIdx, session->m_SeqID.idx);
                    _InterlockedDecrement(&session->m_ioCount);
                }
            }
            break;

            // WSAENOTSOCK
        case 10038:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"is not socket  HANDLE value : %lld   seqID :%llu  seqIndx : %llu\n",
                                           session->m_sock, session->m_SeqID.SeqNumberAndIdx, session->m_SeqID.idx);
            break;
            // WSAECONNABORTED
        case 10054:
        case 10053:
            _InterlockedDecrement(&session->m_ioCount);
            InterlockedExchange(&session->m_blive, 0);
            break;
        default:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                           L"WSASend_failed_ HANDLE value : %lld is not socket  seqID :%llu  seqIndx : %llu GetLastError : % d << \n ",
                                           session->m_sock, session->m_SeqID.SeqNumberAndIdx, session->m_SeqID.idx, LastError);
            _InterlockedDecrement(&session->m_ioCount);
            InterlockedExchange(&session->m_blive, 0);
        }
    }
}

void CLanServer::RecvPacket(clsSession *const session)
{
    ringBufferSize freeSize;
    ringBufferSize directEnQsize;

    int wsaRecv_retval;
    DWORD LastError;
    DWORD flag = 0;
    DWORD bufCnt;
    wchar_t buffer[1000];

    // WSABUF wsaBuf[2];

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

    wsaRecv_retval = WSARecv(session->m_sock, session->m_lastRecvWSABuf, bufCnt, nullptr, &flag, &session->m_recvOverlapped, nullptr);
    if (wsaRecv_retval < 0)
    {
        LastError = GetLastError();
        switch (LastError)
        {
        case WSA_IO_PENDING:
            if (session->m_blive == 0)
            {
                // 함수가 성공하면 반환 값이 0이 아닙니다.
                if (CancelIoEx(m_hIOCP, &session->m_recvOverlapped) == true)
                {
                    StringCchPrintfW(buffer, 30, L"WSARecv CancelIoEx HANDLE value : %lld", session->m_sock);
                    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode, L"%s\n", buffer);
                    _InterlockedDecrement(&session->m_ioCount);
                }
            }
            break;

            // WSAENOTSOCK
        case 10038:
            StringCchPrintfW(buffer, 30, L"HANDLE value : %lld is not socket", session->m_sock);
            ERROR_FILE_LOG(L"Socket_Error.txt", buffer);
            break;
            // WSAECONNABORTED
        case 10054:
        case 10053:
            _InterlockedDecrement(&session->m_ioCount);
            InterlockedExchange(&session->m_blive, 0);
            break;
        default:
            local_IoCount = _InterlockedDecrement(&session->m_ioCount);
            {
                StringCchPrintfW(buffer, sizeof(buffer) / sizeof(wchar_t), L" sessionPrt : %p WSARecv_failed  ioCount %llu GetLastError : %d", session, local_IoCount, LastError);
                CSystemLog::GetInstance()->Log(L"unDefine_Error", en_LOG_LEVEL::ERROR_Mode, L"%s << \n", buffer);
            }
            InterlockedExchange(&session->m_blive, 0);
        }
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
