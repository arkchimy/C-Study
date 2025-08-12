#include "Async_Sync_Complet_OrderTest.h"

HANDLE hIOCPPort, hAcceptThread;

void RecvProcedure(clsSession *const session, DWORD transferred);
void SendProcedure(clsSession *const session, DWORD transferred);

void SendPacket(clsSession *const session);
void RecvPacket(clsSession *const session);

std::list<class clsSession *> sessions;

unsigned AcceptThread(void *arg)
{
    SOCKET client_sock;
    SOCKADDR_IN addr;
    SOCKET listen_sock = *reinterpret_cast<SOCKET *>(arg);
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

        CreateIoCompletionPort((HANDLE)client_sock, hIOCPPort, (ull)session, 0);

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
    DWORD transferred;
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

        GetQueuedCompletionStatus(hIOCPPort, &transferred, &key, &overlapped, INFINITE);
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
            RecvProcedure(session, transferred);
            break;
        case Job_Type::Send:
            SendProcedure(session, transferred);
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
int main()
{
    CLanServer EchoServer;
    st_WSAData wsa;
    int iWorkerCnt;

    HANDLE hWorkerThread[24];

    {
        Parser parser;
        wchar_t addr[16];
        short port;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"ServerAddr", addr, 16);
        parser.GetValue(L"ServerPort", port);
        parser.GetValue(L"iWorkerCnt", iWorkerCnt);

        EchoServer.OnServer(addr, port);
    }
    hIOCPPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
    hAcceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &EchoServer.listen_sock, 0, nullptr);

    for (int i = 0; i < iWorkerCnt; i++)
        hWorkerThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    while (1)
    {
    }
}

void RecvProcedure(clsSession *const session, DWORD transferred)
{
    session->recvBuffer->MoveRear(transferred);
    if (InterlockedCompareExchange(&session->_flag, 1, 0) == 0)
        SendPacket(session);
    RecvPacket(session);
}

void SendProcedure(clsSession *const session, DWORD transferred)
{
    if (18 < transferred)
        __debugbreak();
    session->recvBuffer->MoveFront(transferred);
    SendPacket(session);
}

void SendPacket(clsSession *const session)
{
    BOOL cancle_retval;
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

void RecvPacket(clsSession *const session)
{
    BOOL cancle_retval;
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

// TODO: 서버가 closesocket 하기를 원할 때
/*
*             cancle_retval = CancelIoEx((HANDLE)session->_sock, &session->_recvOverlapped);
            if (cancle_retval == 0)
            {
                if (WSAGetLastError() != ERROR_NOT_FOUND)
                    ERROR_FILE_LOG(L"Socket_Error.txt",
                                   L"CancelIoEx Error \n");
            }
            else
                _InterlockedDecrement(&session->_ioCount);
*/