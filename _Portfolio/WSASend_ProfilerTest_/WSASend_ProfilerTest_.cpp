// WSASend_ProfilerTest_.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <thread>
#include <WS2tcpip.h>
#include <WinSock2.h>

#include <list>
#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/ProfilerProject_MT/CProfileManager.h"

#pragma comment(lib, "ws2_32")


SOCKET listen_sock;
std::list<SOCKET> list_sock;

SRWLOCK srw_list;
HANDLE hIOCP;

int bZeroCopy;
int BUFFER_LEN_MAX = 1000;
int WSABUF_CNT = 1;

using ull = unsigned long long;

struct clsSession
{   
    clsSession(ull id, SOCKET sock)
        : sessionID(id), m_sock(sock)
    {

    }
    ull sessionID;
    SOCKET m_sock;
    OVERLAPPED overlapped;

    char *buffer = (char *)malloc(BUFFER_LEN_MAX);
    WSABUF wsabuf[100];
};
void SendPack(clsSession* session)
{
    DWORD sendRetval,LastError;

    for (int i = 0; i < WSABUF_CNT; i++)
    {
        session->wsabuf[i].len = BUFFER_LEN_MAX / WSABUF_CNT * (i + 1);
        if (i == 0)
            session->wsabuf[i].buf = session->buffer;
        else
            session->wsabuf[i].buf = session->buffer + session->wsabuf[i].len;
    }


    sendRetval = WSASend(session->m_sock, session->wsabuf, WSABUF_CNT, nullptr, 0, &session->overlapped, nullptr);
    if (sendRetval < 0)
    {
        LastError = GetLastError();
        switch (LastError)
        {
        case WSA_IO_PENDING:
            break;
        case 10054:
        case 10053:
            break;

        default:
            __debugbreak();
        }
    }
}
unsigned AcceptThread(void *arg)
    {
    WSAData wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);
    InetPtonW(AF_INET, L"127.0.0.1", &addr.sin_addr);
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    bind(listen_sock, (sockaddr *)&addr, sizeof(addr));
    
 

    InitializeSRWLock(&srw_list);

    hIOCP = (HANDLE) CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, 0);
    listen(listen_sock, SOMAXCONN_HINT(65535));
    SOCKADDR_IN clientAddr;
    SOCKET client_sock;
    int len = sizeof(clientAddr);


    while (1)
    {
        static ull id = 1;

        client_sock = accept(listen_sock, (sockaddr *)&clientAddr, &len);

        clsSession* session = new clsSession(client_sock, id++);
        {

            CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

            AcquireSRWLockExclusive(&srw_list);

            list_sock.push_back(client_sock);

            ReleaseSRWLockExclusive(&srw_list);

        }

        SendPack(session);
    }

    return 0;
}
unsigned WorkerThread(void *arg)
{
    size_t *arrArg = reinterpret_cast<size_t *>(arg);
    DWORD transferred;

    ull key;
    OVERLAPPED *overlapped;
    clsSession *session;

    ull local_ioCount;
    DWORD sendRetval;

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
        session = reinterpret_cast<clsSession *>(key);

        if (session == nullptr)
            __debugbreak();
        SendPack(session);

    }
    return 0;
}
int main()
{
    int WokerThreadCnt = 0;

    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"ZeroCopy", bZeroCopy);
        parser.GetValue(L"WokerThreadCnt", WokerThreadCnt);
        parser.GetValue(L"BUFFER_LEN_MAX", BUFFER_LEN_MAX);
        parser.GetValue(L"WSABUF_CNT", WSABUF_CNT);

    }
    _beginthreadex(nullptr, 0, AcceptThread, nullptr, 0, nullptr);
    Sleep(1000);
    for (int i = 0; i < WokerThreadCnt; i++)
        _beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

    while (1)
    {


    }
}
