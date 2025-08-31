// #include "pch.h"

#include "CNetwork_lib.h"

#include "../../../_1Course/lib/Profiler_lib/Profiler_lib.h"
#include "stHeader.h"

#include "../../../error_log.h"
#include <list>
#include <thread>


st_WSAData::st_WSAData()
{
    WSAData wsa;
    DWORD wsaStartRetval;

    wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsaStartRetval != 0)
    {
        ERROR_FILE_LOG(L"WSAData_Error.txt", L"WSAStartup retval is not Zero ");
        __debugbreak();
    }
}

st_WSAData::~st_WSAData()
{
    WSACleanup();
}

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

    ull session_id = 10;
    ull idx;

    clsSession *session;
    CLanServer *server;

    int addrlen;
    stSessionId stsessionID;
    wchar_t buffer[100];

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
            stSRWLock srw = stSRWLock(&server->srw_session_idleList);

            if (server->m_idleIdx.empty() == true)
            {
                // 비어있는 세션이 없을 경우.. 대기열을 만들어야할 듯
                // 일단은 연결을 끊고 계속 Accept  받는 방식으로 하자.
                closesocket(client_sock);
                __debugbreak();
                continue;
            }

            idx = server->m_idleIdx.front();
            server->m_idleIdx.pop_front();
        }

        stsessionID.idx = idx;
        stsessionID.seqNumber = session_id++;

        session = &server->sessions_vec[idx];
        session->m_SeqID.SeqNumberAndIdx = stsessionID.SeqNumberAndIdx;
        session->m_sock = client_sock;

        InterlockedExchange(&session->m_sock, client_sock);
        InterlockedExchange(&session->m_SeqID.SeqNumberAndIdx, stsessionID.SeqNumberAndIdx);

        server->OnAccept(session->m_SeqID.SeqNumberAndIdx, addr);

        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

        server->RecvPacket(session);
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
    DWORD transferred;

    HANDLE hIOCP = (HANDLE)*arrArg;
    CLanServer *server = reinterpret_cast<CLanServer *>(arrArg[1]);

    ull key;
    OVERLAPPED *overlapped;
    clsSession *session;

    ull local_ioCount;

    while (1)
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
            server->RecvComplete(session, transferred);
            break;
        case Job_Type::Send:
            server->SendComplete(session, transferred);
            break;

        case Job_Type::PostRecv:
            InterlockedDecrement((unsigned long long *)&session->m_RcvPostCnt);
            server->RecvPostComplete(session, transferred);
            break;

        case Job_Type::PostSend:
            server->PostComplete(session, transferred);
            break;

        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
            __debugbreak();
        }
        local_ioCount = InterlockedDecrement(&session->m_ioCount);

        if (local_ioCount == 0)
        {
            if (InterlockedCompareExchange(&session->m_blive, false, true) == false)
                ERROR_FILE_LOG(L"Critical_Error.txt", L"socket_live Change Failed");
        }
    }

    return 0;
}
BOOL CLanServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions, int ZeroByteTest)
{
    linger linger;
    int buflen;
    DWORD lProcessCnt;
    DWORD bind_retval;

    SOCKADDR_IN serverAddr;

    sessions_vec.resize(MaxSessions);

    for (ull idx = 0; idx < MaxSessions; idx++)
        m_idleIdx.push_back(idx);

    linger.l_onoff = 1;
    linger.l_linger = 0;
    m_ZeroByteTest = ZeroByteTest;

    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    InetPtonW(AF_INET, bindAddress, &serverAddr.sin_addr);

    m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);

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

    if (m_listen_sock == INVALID_SOCKET)
    {

        ERROR_FILE_LOG(L"Socket_Error.txt",
                       L"listen_sock Create Socket Error");
        return false;
    }

    if (GetLogicalProcess(lProcessCnt) == false)
        ERROR_FILE_LOG(L"GetLogicalProcessError.txt", L"GetLogicalProcess_Error");

    m_hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, lProcessCnt - reduceThreadCount);

    m_hThread = new HANDLE[WorkerCreateCnt];

    HANDLE WorkerArg[] = {m_hIOCP, this};
    HANDLE AcceptArg[] = {m_hIOCP, (HANDLE)m_listen_sock, this};
    // TODO : 넘겨줄 매개변수를  지역변수로 ???  다른 방안 생각하기

    m_hAccept = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &AcceptArg, 0, nullptr);
    for (int i = 0; i < WorkerCreateCnt; i++)
        m_hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &WorkerArg, 0, nullptr);
    Sleep(1000);// 지역변수가 반환 되므로 Sleep을 했음. 
    return true;
}

void CLanServer::Stop()
{
}

int CLanServer::GetSessionCount()
{

    return 0;
}

bool CLanServer::Disconnect(clsSession *const session)
{
    return false;
}

void CLanServer::RecvComplete(clsSession *const session, DWORD transferred)
{
    session->m_recvBuffer.MoveRear(transferred);

    stHeader header;
    ringBufferSize useSize;
    float qPersentage;

    char *f, *r;
    f = session->m_recvBuffer._frontPtr;
    r = session->m_recvBuffer._rearPtr;

    // ContentsQ의 상황이 어떤지를 체크.
    qPersentage = OnRecv((ull)(session->m_SeqID.SeqNumberAndIdx), nullptr);
    if (qPersentage >= 75.f)
    {
        RecvPostMessage(session);
        return;
    }
    // Header의 크기만큼을 확인.
    while (session->m_recvBuffer.Peek(&header, sizeof(stHeader), f, r) == sizeof(stHeader))
    {
        useSize = session->m_recvBuffer.GetUseSize(f, r);
        if (useSize < header._len + sizeof(stHeader))
        {
            break;
        }
        // 메세지 생성
        CMessage *msg = CreateCMessage(session, header);
        qPersentage = OnRecv(session->m_SeqID.SeqNumberAndIdx, msg);
        f = session->m_recvBuffer._frontPtr;
        if (qPersentage >= 75.0)
        {
            RecvPostMessage(session);
            return;
        }
    }
    useSize = session->m_recvBuffer.GetUseSize();
    if (useSize >= 10)
    {
        RecvPostMessage(session);
        return;
    }

    RecvPacket(session);
}

CMessage *CLanServer::CreateCMessage(clsSession *const session, class stHeader &header)
{
    CMessage *msg = &CObjectPoolManager::pool.Alloc()->data;
    // ringBufferSize directDeQsize;

    // directDeQsize = session->m_recvBuffer.DirectDequeueSize(f, r);
    unsigned short type = 0;
    ringBufferSize deQsize;

    switch (type)
    {
    default:
    {
        deQsize = session->m_recvBuffer.Dequeue(msg->_begin, sizeof(header) + header._len);
        msg->_rearPtr = msg->_begin + deQsize;
        msg->_begin = msg->_begin;
    }
    }
    return msg;
}
void CLanServer::SendComplete(clsSession *const session, DWORD transferred)
{
    // session->m_sendBuffer.MoveFront(transferred);
    size_t m_SendMsgSize = session->m_SendMsg.size();

    for (CMessage *msg : session->m_SendMsg)
    {
        CObjectPoolManager::pool.Release(msg);
        
    }
    session->m_SendMsg.clear();
    SendPacket(session);
}
void CLanServer::RecvPostComplete(class clsSession *const session, DWORD transferred)
{

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
        RecvPostMessage(session);
        return;
    }
    // Header의 크기만큼을 확인.
    while (session->m_recvBuffer.Peek(&header, sizeof(stHeader), f, r) == sizeof(stHeader))
    {
        useSize = session->m_recvBuffer.GetUseSize(f, r);
        if (useSize < header._len + sizeof(stHeader))
        {
            break;
        }
        // 메세지 생성
        CMessage *msg = CreateCMessage(session, header);
        qPersentage = OnRecv(session->m_SeqID.SeqNumberAndIdx, msg);
        f = session->m_recvBuffer._frontPtr;
        if (qPersentage >= 75.f)
        {
            RecvPostMessage(session);
            return;
        }
    }
    useSize = session->m_recvBuffer.GetUseSize();
    if (useSize >= 10)
    {
        RecvPostMessage(session);
        return;
    }
    RecvPacket(session);
}
void CLanServer::PostComplete(clsSession *const session, DWORD transferred)
{
    if (_InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
    {
        _InterlockedCompareExchange(&session->m_Postflag, 0, 1);
        SendPacket(session);
        return;
    }
    SendPostMessage(session->m_SeqID.SeqNumberAndIdx);
}

void CLanServer::SendPacket(clsSession *const session)
{
    ringBufferSize useSize, directDQSize;
    ringBufferSize deQSize;

    DWORD bufCnt;
    int send_retval;

    WSABUF wsaBuf[500];
    DWORD LastError;

    useSize = session->m_sendBuffer.GetUseSize();

    if (useSize == 0)
    {
        if (!m_ZeroByteTest)
        {
            //// flag를 끄고 Recv를 받은 후에 완료통지를 왔다면
            _InterlockedCompareExchange(&session->m_flag, 0, 1);
            return;
        }
    }

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
    }
    directDQSize = session->m_sendBuffer.DirectDequeueSize();
    ull msgAddr;
    CMessage *msg;

    bufCnt = 0;

    while (useSize >= 8)
    {
        {
            deQSize = session->m_sendBuffer.Peek(&msgAddr, sizeof(size_t));
            msg = reinterpret_cast<CMessage *>(msgAddr);
            wsaBuf[bufCnt].buf = msg->_frontPtr;
            wsaBuf[bufCnt].len = msg->_rearPtr - msg->_frontPtr;
        }
        deQSize = session->m_sendBuffer.Dequeue(&msgAddr, sizeof(size_t));
        if (deQSize != sizeof(size_t))
            __debugbreak();

        msg = reinterpret_cast<CMessage *>(msgAddr);
        session->m_SendMsg.push_back(msg);

        wsaBuf[bufCnt].buf = msg->_frontPtr;
        wsaBuf[bufCnt].len = msg->_rearPtr - msg->_frontPtr;
        bufCnt++;
        useSize -= 8;
    }
    if (bufCnt == 0)
    {
        //// flag를 끄고 Recv를 받은 후에 완료통지를 왔다면
        _InterlockedCompareExchange(&session->m_flag, 0, 1);
        return;
    }
    _InterlockedIncrement(&session->m_ioCount);

    if (bZeroCopy)
    {
        Profile profile(L"ZeroCopy WSASend");
        send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
    }
    else
    {
        Profile profile(L"WSASend");
        send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
    }
    if (send_retval < 0)
    {
        LastError = GetLastError();
        if (LastError != WSA_IO_PENDING)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"WSASend Error ");
            _InterlockedDecrement(&session->m_ioCount);
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
    wchar_t buffer[100];

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

    _InterlockedIncrement(&session->m_ioCount);

    wsaRecv_retval = WSARecv(session->m_sock, session->m_lastRecvWSABuf, bufCnt, nullptr, &flag, &session->m_recvOverlapped, nullptr);
    if (wsaRecv_retval < 0)
    {
        LastError = GetLastError();
        switch (LastError)
        {
        case WSA_IO_PENDING:
            StringCchPrintfW(buffer, 30, L"Socket %lld WSARecv WSA_IO_PENDING", session->m_sock);

            break;
        case 10054:
        case 10053:
            break;

        default:
            _InterlockedDecrement(&session->m_ioCount);

            StringCchPrintfW(buffer, 100, L"Socket %lld WSARecv Failed", session->m_sock);
        }
    }
    else
    {
        StringCchPrintfW(buffer, 100, L"Socket %lld WSARecv Sync", session->m_sock);
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
CLanServer::~CLanServer()
{
    closesocket(m_listen_sock);
}
