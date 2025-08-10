#include "Async_Sync_Complet_OrderTest.h"

HANDLE hIOCPPort, hAcceptThread;

void RecvProcedure(clsSession *const session, DWORD transferred);
void SendProcedure(clsSession *const session, DWORD transferred);
void PostProcedure(clsSession *const session);

void SendPacket(clsSession *const session);
void RecvPacket(clsSession *const session);

unsigned AcceptThread(void *arg)
{
    SOCKET client_sock;
    SOCKADDR_IN addr;
    SOCKET listen_sock = *reinterpret_cast<SOCKET *>(arg);
    DWORD listen_retval;

    int addrlen = sizeof(addr);

    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
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

        CreateIoCompletionPort((HANDLE)client_sock, hIOCPPort, (ull)session, 0);
        WSABUF wsabuf;
        DWORD flag = 0;
        int wsarecv_retval;

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
                closesocket(client_sock);
                delete session;
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
        GetQueuedCompletionStatus(hIOCPPort, &transferred, &key, &overlapped, INFINITE);
        if (overlapped == nullptr)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"GetQueuedCompletionStatus Overlapped is nullptr\n");
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
        case Job_Type::PostSend:
            PostProcedure(session);
            break;
        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
        }

        local_ioCount = InterlockedDecrement(&session->_ioCount);
        if (local_ioCount == 0)
        {
            closesocket(session->_ioCount);
            delete session;
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
    stEchoHeader header;
    ringBufferSize useSize, freeSize;
    ringBufferSize DeQsize, payLoadDeQsize;
    ringBufferSize directDeQsize;
    int wsaRecv_retval;

    session->recvBuffer->MoveRear(transferred);
    while (1)
    {
        useSize = session->recvBuffer->GetUseSize();
        freeSize = session->sendBuffer->GetFreeSize(); // SendBuffer에 바로넣기 위함.
        // Peek 의 반환값은 useSize가 더 적다면 useSize를 보낸다.
        if (useSize < sizeof(header))
            break;

        if (session->recvBuffer->Peek(&header, sizeof(header)) == sizeof(header))
        {
            // payLoad만큼 왔다면
            if (useSize < sizeof(header) + header.len)
                break;
            // SendBuffer에 빈 공간이 부족하다면
            if (freeSize < sizeof(header) + header.len)
                break;
            DeQsize = session->recvBuffer->Dequeue(session->sendBuffer->_rearPtr, header.len + sizeof(stEchoHeader));
            session->sendBuffer->MoveRear(DeQsize);

            if (DeQsize != header.len + sizeof(stEchoHeader))
            {
                ERROR_FILE_LOG(L"Critical_Error.txt", L"DeQueueSize_Dif_retvalue");
                __debugbreak();
            }
        }
    }

    if (InterlockedCompareExchange(&session->_flag, 1, 0) == 0)
        SendPacket(session);
    RecvPacket(session);
}

void SendProcedure(clsSession *const session, DWORD transferred)
{
    DWORD useSize;

    session->sendBuffer->MoveFront(transferred);
    useSize = session->sendBuffer->GetUseSize();

    //if (useSize == 0)
    //{
    //    // flag 를 종료 Post를 통해 useSize의 오진단을 막는 시도.
    // 
    //    if (InterlockedCompareExchange(&session->_flag, 0, 1) == 1)
    //    {
    //        ZeroMemory(&session->_PostOverlapped, sizeof(stOverlapped));
    //        session->_PostOverlapped._mode = Job_Type::PostSend;

    //        _InterlockedIncrement(&session->_ioCount);
    //        PostQueuedCompletionStatus(hIOCPPort, 0, (ULONG_PTR)session, &session->_PostOverlapped);
    //    }
    //    return;
    //}

    SendPacket(session);
}

void PostProcedure(clsSession *const session)
{
    if (InterlockedCompareExchange(&session->_flag, 1, 0) == 0)
    {
        SendPacket(session);
    }
}

void SendPacket(clsSession *const session)
{
    DWORD useSize, directDQSize;

    DWORD bufCnt = 2;
    int send_retval;

    WSABUF wsaBuf[2];

    useSize = session->sendBuffer->GetUseSize();

    //if (useSize == 0)
    //    return;
    directDQSize = session->sendBuffer->DirectDequeueSize();

    ZeroMemory(wsaBuf, sizeof(wsaBuf));
    ZeroMemory(&session->_sendOverlapped, sizeof(stOverlapped));
    session->_sendOverlapped._mode = Job_Type::Send;

 
    wsaBuf[0].buf = session->sendBuffer->_frontPtr;
    wsaBuf[0].len = directDQSize;

    wsaBuf[1].buf = session->sendBuffer->_begin;
    wsaBuf[1].len = useSize - directDQSize;

    bufCnt = 2;
    
    _InterlockedIncrement(&session->_ioCount);

    send_retval = WSASend(session->_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session->_sendOverlapped, nullptr);
    if (send_retval == -1)
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt",
                           L"WSASend Error \n");
            _InterlockedDecrement(&session->_ioCount);
        }
    }
}

void RecvPacket(clsSession *const session)
{

    ringBufferSize useSize, freeSize;
    ringBufferSize directEnQsize;

    int wsaRecv_retval;
    
    DWORD flag = 0;
    DWORD bufCnt = 2;

    WSABUF wsaBuf[2];

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session->_recvOverlapped, sizeof(stOverlapped));
        session->_recvOverlapped._mode = Job_Type::Recv;
    }

    freeSize = session->recvBuffer->GetFreeSize(); // SendBuffer에 바로넣기 위함.
    directEnQsize = session->recvBuffer->DirectEnqueueSize();


    bufCnt = 2;
    wsaBuf[0].buf = session->recvBuffer->_rearPtr;
    wsaBuf[0].len = directEnQsize;

    wsaBuf[1].buf = session->recvBuffer->_begin;
    wsaBuf[1].len = freeSize - directEnQsize;
    

    _InterlockedIncrement(&session->_ioCount);
    wsaRecv_retval = WSARecv(session->_sock, wsaBuf, bufCnt, nullptr, &flag, &session->_recvOverlapped, nullptr);
    if (wsaRecv_retval < 0)
    {
        DWORD LastError = GetLastError();
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
