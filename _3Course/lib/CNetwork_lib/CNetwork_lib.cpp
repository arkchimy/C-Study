//#include "pch.h"
#include "CNetwork_lib.h"
#include "../../../error_log.h"
#include "clsSession.h"

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

    for (int i = 0; i < cnt; i++)
    {
        if (infos[i].Relationship == RelationProcessorCore)
        {
            DWORD mask = infos[i].ProcessorMask;
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

std::list<class clsSession *> sessions;

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
        sessions.push_back(session);

        for (std::list<clsSession *>::iterator iter = sessions.begin(); iter != sessions.end(); iter++)
        {
            if ((*iter)->_blive == false)
            {
                delete *iter;
                iter = sessions.erase(iter);
            }
        }

        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

        wsabuf.buf = session->recvBuffer->_rearPtr;
        wsabuf.len = session->recvBuffer->GetFreeSize();

        _InterlockedIncrement(&session->_ioCount);

        wsarecv_retval = WSARecv(client_sock, &wsabuf, 1, nullptr, &flag, &session->_recvOverlapped, nullptr);
        if (wsarecv_retval < 0)
        {
            if (WSA_IO_PENDING != GetLastError())
            {
                _InterlockedDecrement(&session->_ioCount);
                ERROR_FILE_LOG(L"Socket_Error.txt", L"Accept Recv Error");

                session->_blive = false;
            }
        }
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
        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
        }
        local_ioCount = InterlockedDecrement(&session->_ioCount);

        if (local_ioCount == 0)
        {
            if (InterlockedCompareExchange(&session->_blive, false, true) == false)
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
    HANDLE AcceptArg[] = {m_hIOCP, (HANDLE)m_listen_sock};

    m_hAccept = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &AcceptArg, 0, nullptr);
    for (DWORD i = 0; i < WorkerCreateCnt; i++)
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
    session->recvBuffer->MoveRear(transferred);
    if (InterlockedCompareExchange(&session->_flag, 1, 0) == 0)
        SendPacket(session);
    RecvPacket(session);
}

void CLanServer::SendComplete(clsSession *const session, DWORD transferred)
{
    if (18 < transferred)
        __debugbreak();
    session->recvBuffer->MoveFront(transferred);
    SendPacket(session);
}

void CLanServer::SendPacket(clsSession *const session)
{
    ringBufferSize useSize, directDQSize;
    DWORD bufCnt;
    int send_retval;

    WSABUF wsaBuf[2];
    DWORD LastError;

    char *f = session->recvBuffer->_frontPtr, *r = session->recvBuffer->_rearPtr;

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->_sendOverlapped, sizeof(OVERLAPPED));
    }
    directDQSize = session->recvBuffer->DirectDequeueSize(f, r);
    useSize = session->recvBuffer->GetUseSize(f, r);

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

        wsaBuf[1].buf = session->recvBuffer->_begin;
        wsaBuf[1].len = useSize - directDQSize;

        bufCnt = 2;
    }

    _InterlockedIncrement(&session->_ioCount);

    send_retval = WSASend(session->_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->_sendOverlapped, nullptr);
    if (send_retval < 0)
    {
        LastError = GetLastError();
        if (LastError != WSA_IO_PENDING)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"WSASend Error ");
            _InterlockedDecrement(&session->_ioCount);
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

    char *f = session->recvBuffer->_frontPtr, *r = session->recvBuffer->_rearPtr;

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->_recvOverlapped, sizeof(OVERLAPPED));
    }

    directEnQsize = session->recvBuffer->DirectEnqueueSize(f, r);
    freeSize = session->recvBuffer->GetFreeSize(f, r); // SendBuffer에 바로넣기 위함.

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

        wsaBuf[1].buf = session->recvBuffer->_begin;
        wsaBuf[1].len = freeSize - directEnQsize;

        bufCnt = 2;
    }

    _InterlockedIncrement(&session->_ioCount);

    wsaRecv_retval = WSARecv(session->_sock, wsaBuf, bufCnt, nullptr, &flag, &session->_recvOverlapped, nullptr);
    if (wsaRecv_retval < 0)
    {
        LastError = GetLastError();
        switch (LastError)
        {
        case WSA_IO_PENDING:
            break;
        default:
            _InterlockedDecrement(&session->_ioCount);
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
