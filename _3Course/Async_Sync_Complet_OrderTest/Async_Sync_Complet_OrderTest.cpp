#include "Async_Sync_Complet_OrderTest.h"


HANDLE hIOCPPort, hAcceptThread;

void RecvProcedure(clsSession *session, DWORD transferred);
void SendProcedure(clsSession *session, DWORD transferred);

unsigned AcceptThread(void *arg)
{
    SOCKET client_sock;
    SOCKADDR_IN addr;
    SOCKET listen_sock = *reinterpret_cast<SOCKET *>(arg);
    int addrlen = sizeof(addr);

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

        AcquireSRWLockExclusive(&session->srw_session);
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
        ReleaseSRWLockExclusive(&session->srw_session);

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

    {
        Parser parser;
        wchar_t addr[16];
        short port;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"ServerAddr", addr, 16);
        parser.GetValue(L"ServerPort", port);

        EchoServer.OnServer(addr, port);
    }
    hIOCPPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
    hAcceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &EchoServer.listen_sock, 0, nullptr);
}

void RecvProcedure(clsSession *session, DWORD transferred)
{
    stEchoHeader header;
    ringBufferSize useSize,freeSize;
    ringBufferSize DeQsize, payLoadDeQsize;
    ringBufferSize directDeQsize;
    DWORD wsaRecv_retval ,flag = 0;

    session->recvBuffer->MoveRear(transferred);
    while (1)
    {
        useSize = session->recvBuffer->GetUseSize();
        freeSize = session->sendBuffer->GetFreeSize(); // SendBuffer에 바로넣기 위함.
        // Peek 의 반환값은 useSize가 더 적다면 useSize를 보낸다.
        if (session->recvBuffer->Peek(&header, sizeof(header)) == sizeof(header))
        {
            // payLoad만큼 왔다면
            if (useSize < sizeof(header) + header.len)
                break;
            // SendBuffer에 빈 공간이 부족하다면
            if (freeSize < sizeof(header) + header.len)
                break;
            DeQsize = session->recvBuffer->Dequeue(session->sendBuffer->_rearPtr, header.len + sizeof(stEchoHeader));
            if (DeQsize != header.len + sizeof(stEchoHeader))
            {
                ERROR_FILE_LOG(L"Critical_Error.txt", "DeQueueSize_Dif_retvalue");
                __debugbreak();
            }
        }

    }
    directDeQsize = session->recvBuffer->DirectDequeueSize();

    {
        WSABUF wsaBuffer[2];
        DWORD bufCnt;

        ZeroMemory(wsaBuffer, sizeof(wsaBuffer));
        ZeroMemory(&session->_recvOverlapped, sizeof(stOverlapped));

        if (freeSize < directDeQsize)
        {
            bufCnt = 1;
            wsaBuffer[0].buf = session->recvBuffer->_rearPtr;
            wsaBuffer[0].len = freeSize;
        }
        else
        {
            bufCnt = 2;
            wsaBuffer[0].buf = session->recvBuffer->_rearPtr;
            wsaBuffer[0].len = freeSize;

            wsaBuffer[1].buf = session->recvBuffer->_rearPtr;
            wsaBuffer[1].len = freeSize;
        }
        _InterlockedIncrement(&session->_ioCount);
        wsaRecv_retval = WSARecv(session->_sock, wsaBuffer, bufCnt, nullptr, &flag, &session->_recvOverlapped, nullptr);
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
   
    
}

void SendProcedure(clsSession *session, DWORD transferred)
{
    DWORD useSize;
    DWORD directDQSize;
    WSABUF wsaBuf[2];
    DWORD bufCnt = 2;
    DWORD send_retval;

    session->sendBuffer->MoveFront(transferred);

    useSize = session->sendBuffer->GetUseSize();
    directDQSize = session->sendBuffer->DirectDequeueSize();

    if (useSize == 0)
        return;

    if (directDQSize > useSize)
    {
        wsaBuf[0].buf = session->sendBuffer->_frontPtr;
        wsaBuf[0].len = useSize;

        bufCnt = 1;
    }
    else
    {
        wsaBuf[0].buf = session->sendBuffer->_frontPtr;
        wsaBuf[0].len = directDQSize;

        wsaBuf[1].buf = session->sendBuffer->_begin;
        wsaBuf[1].len = useSize - directDQSize;

        bufCnt = 2;
    }
    _InterlockedIncrement(&session->_ioCount);
    ZeroMemory(&session->_sendOverlapped, sizeof(stOverlapped));

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
