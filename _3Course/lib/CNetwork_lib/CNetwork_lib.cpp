// #include "pch.h"
#include "CNetwork_lib.h"

#include "stHeader.h"

#include <list>
#include <thread>
#include "../../../error_log.h"

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

    WSABUF wsabuf;
    DWORD flag;
    ull session_id = 1;

    CLanServer *server = reinterpret_cast<CLanServer *>(arrArg[2]);

    int wsarecv_retval;
    int addrlen = sizeof(addr);

    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
    flag = 0;

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

        clsSession *session = new clsSession(client_sock);

        EnterCriticalSection(&server->cs_sessionMap);
        server->sessions[session_id] = session;
        session->m_id = session_id++;
        

        for (std::map<ull,clsSession *>::iterator iter = server->sessions.begin(); iter != server->sessions.end(); iter++)
        {
            clsSession *element = iter->second;
            if (element->m_blive == false)
            {
                delete element;
                iter = server->sessions.erase(iter);
            }
        }
        LeaveCriticalSection(&server->cs_sessionMap);


        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);
        server->RecvPacket(session);

        //wsabuf.buf = session->m_recvBuffer->_rearPtr;
        //wsabuf.len = session->m_recvBuffer->GetFreeSize();

        //_InterlockedIncrement(&session->m_ioCount); 
  /*      wsarecv_retval = WSARecv(client_sock, &wsabuf, 1, nullptr, &flag, &session->m_recvOverlapped, nullptr);
        if (wsarecv_retval < 0)
        {
            if (WSA_IO_PENDING != GetLastError())
            {
                _InterlockedDecrement(&session->m_ioCount);
                ERROR_FILE_LOG(L"Socket_Error.txt", L"Accept Recv Error");

                session->m_blive = false;
            }
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
            // session = nullptr;
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
        case Job_Type::PostSend:
            server->PostComplete(session, transferred);
            break;
        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
        }
        local_ioCount = InterlockedDecrement(&session->m_ioCount);

        if (local_ioCount == 0)
        {
            if (InterlockedCompareExchange(&session->m_blive, false, true) == false)
                ERROR_FILE_LOG(L"Critical_Error.txt", L"socket_live Change Failed");
            // InterlockedExchange(&session->_blive, false);
        }
    }

    return 0;
}
BOOL CLanServer::Start(const wchar_t *bindAddress, short port, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions)
{
    linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;

    DWORD lProcessCnt;
    DWORD bind_retval;

    SOCKADDR_IN serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    InetPtonW(AF_INET, bindAddress, &serverAddr.sin_addr);

    m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);

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
    HANDLE AcceptArg[] = {m_hIOCP, (HANDLE)m_listen_sock,this};

    m_hAccept = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &AcceptArg, 0, nullptr);
    for (int i = 0; i < WorkerCreateCnt; i++)
        m_hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &WorkerArg, 0, nullptr);
    Sleep(10000);
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
    session->m_recvBuffer->MoveRear(transferred);

    stHeader header;
    ringBufferSize useSize;
    float qPersentage;

    // Header의 크기만큼을 확인.
    while (session->m_recvBuffer->Peek(&header, sizeof(stHeader)) == sizeof(stHeader))
    {
        useSize = session->m_recvBuffer->GetUseSize();
        if (useSize < header._len + sizeof(stHeader))
        {
            break;
        }
        session->m_recvBuffer->MoveFront(sizeof(stHeader));
        // 메세지 생성
        CMessage *msg = CreateCMessage(session, header);
        qPersentage = OnRecv(session->m_id, msg);

        if (qPersentage >= 75.f)
            break;
    }
    RecvPacket(session);
}

CMessage *CLanServer::CreateCMessage(clsSession *const session, class stHeader &header)
{
    CMessage *msg = new CMessage();
    // ringBufferSize directDeQsize;

    // directDeQsize = session->m_recvBuffer->DirectDequeueSize(f, r);
    *msg << session->m_id;

    switch (header._len)
    {
    default:
    {
        short len;
        char payload[8];
        session->m_recvBuffer->Dequeue(&len, sizeof(len));
        session->m_recvBuffer->Dequeue(&payload, sizeof(payload));
        *msg << len;
        msg->PutData(payload, sizeof(payload));
    }
    }
    return msg;
}
void CLanServer::SendComplete(clsSession *const session, DWORD transferred)
{
    if (18 < transferred)
        __debugbreak();
    session->m_recvBuffer->MoveFront(transferred);
    InterlockedCompareExchange(&session->m_flag, 0, 1);
}

void CLanServer::PostComplete(clsSession *const session, DWORD transferred)
{
    char *f = session->m_recvBuffer->_frontPtr, *r = session->m_recvBuffer->_rearPtr;

    ringBufferSize usesize;
    usesize = session->m_recvBuffer->GetUseSize();
    if (usesize == 0)
    {
        return;
    }
    if (_InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
    {
        SendPacket(session);
    }
}

void CLanServer::SendPacket(clsSession *const session)
{
    ringBufferSize useSize, directDQSize;
    DWORD bufCnt;
    int send_retval;

    WSABUF wsaBuf[2];
    DWORD LastError;

    char *f = session->m_sendBuffer->_frontPtr, *r = session->m_sendBuffer->_rearPtr;

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
    }
    useSize = session->m_sendBuffer->GetUseSize(f, r);

    if (useSize == 0)
    {
        //// flag를 끄고 Recv를 받은 후에 완료통지를 왔다면
        //if (_InterlockedCompareExchange(&session->m_flag, 0, 1) == 1)
        //{
        //    InterlockedIncrement(&session->m_ioCount);
        //    PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_postOverlapped);
        //}
        return;
    }
    directDQSize = session->m_sendBuffer->DirectDequeueSize(f, r);

    if (useSize <= directDQSize)
    {
        wsaBuf[0].buf = f;
        wsaBuf[0].len = useSize;

        bufCnt = 1;
    }
    else
    {
        wsaBuf[0].buf = f;
        wsaBuf[0].len = directDQSize;

        wsaBuf[1].buf = session->m_sendBuffer->_begin;
        wsaBuf[1].len = useSize - directDQSize;

        bufCnt = 2;
    }

    _InterlockedIncrement(&session->m_ioCount);

    send_retval = WSASend(session->m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->m_sendOverlapped, nullptr);
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

    WSABUF wsaBuf[2];

    char *f = session->m_recvBuffer->_frontPtr, *r = session->m_recvBuffer->_rearPtr;

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->m_recvOverlapped, sizeof(OVERLAPPED));
    }

    directEnQsize = session->m_recvBuffer->DirectEnqueueSize(f, r);
    freeSize = session->m_recvBuffer->GetFreeSize(f, r); // SendBuffer에 바로넣기 위함.

    if (freeSize <= directEnQsize)
    {
        wsaBuf[0].buf = r;
        wsaBuf[0].len = directEnQsize;

        bufCnt = 1;
    }
    else
    {
        wsaBuf[0].buf = r;
        wsaBuf[0].len = directEnQsize;

        wsaBuf[1].buf = session->m_recvBuffer->_begin;
        wsaBuf[1].len = freeSize - directEnQsize;

        bufCnt = 2;
    }

    _InterlockedIncrement(&session->m_ioCount);

    wsaRecv_retval = WSARecv(session->m_sock, wsaBuf, bufCnt, nullptr, &flag, &session->m_recvOverlapped, nullptr);
    if (wsaRecv_retval < 0)
    {
        LastError = GetLastError();
        switch (LastError)
        {
        case WSA_IO_PENDING:
            break;
        default:
            _InterlockedDecrement(&session->m_ioCount);
            ERROR_FILE_LOG(L"Socket_Error.txt", L"WSARecv_Error");
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
CLanServer::~CLanServer()
{
    closesocket(m_listen_sock);
}
