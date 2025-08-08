#include "st_WSAData.h"

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <thread>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../error_log.h"

#include "CLanServer.h"
#include "clsSession.h"

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

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
            ERROR_FILE_LOG(L"SocketError.txt", L"Accept Error ");
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
            ERROR_FILE_LOG(L"SocketError.txt", L"GetQueuedCompletionStatus Overlapped is nullptr\n");
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
            ERROR_FILE_LOG(L"SocketError.txt", L"UnDefine Error Overlapped_mode");
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
    session->recvBuffer->MoveRear(transferred);
    
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

            ERROR_FILE_LOG(L"SocketError.txt",
                           L"WSASend Error \n");
            _InterlockedDecrement(&session->_ioCount);
        }
    }
}
