// WSASend_ProfilerTest_.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>
#include <thread>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include <strsafe.h>

#include "CLargePage.h"
#include <assert.h>
#include <list>
#include <vector>

#pragma comment(lib, "ws2_32")

using ull = unsigned long long;

SOCKET listen_sock;
std::list<SOCKET> list_sock;

SRWLOCK srw_list;
HANDLE hIOCP;
HANDLE hExitEvent;
HANDLE hWSASend_Overapped;

int bZeroCopy = 0;
int BUFFER_LEN_MAX = 1000;
int WSABUF_CNT = 1;
int LargePage;

std::vector<ull> TPS_vec; // TPS의 전에 측정된 값

#define MAX_LEN_BUFFER 1024 * 1024 * 10
#define LOOP_CNT 50


enum class en_SendMode
{
    FirstSend,
    SecondSend,
    MAX,
};
struct MyOVERLAPPED : public OVERLAPPED
{
    MyOVERLAPPED(en_SendMode mode) : _mode(mode) 
    {
        _id = _interlockedincrement64(&s_id);
    }

    long long _id = 0;
    inline static long long s_id = -1;
    en_SendMode _mode;
};

struct clsSession
{
    clsSession(ull id, SOCKET sock)
        : sessionID(id), m_sock(sock)
    {
        for (int i = 0; i < LOOP_CNT; i++)
        {
            buffer[i] = i;
        }
    }
    ull sessionID;
    SOCKET m_sock;
    char buffer[LOOP_CNT];
};

void SendPack(clsSession *session)
{
    int sendRetval, LastError;
    int Async_sendRetval, Async_LastError;

    DWORD flag = 0;

    LastError = GetLastError();
    MyOVERLAPPED *overlapped;
    {
        WSABUF wsabuf;
        wsabuf.buf = (char *)malloc(MAX_LEN_BUFFER);
        memset(wsabuf.buf, 0xEE, MAX_LEN_BUFFER);

        wsabuf.len = MAX_LEN_BUFFER;

        overlapped = new MyOVERLAPPED(en_SendMode::FirstSend);
        Async_sendRetval = WSASend(session->m_sock, &wsabuf, 1, nullptr, 0, overlapped, nullptr);
        LastError = GetLastError();

        if (Async_sendRetval != 0)
        {

            switch (LastError)
            {
            case WSA_IO_PENDING:
                printf("%lld WSA_IO_PENDING \n", overlapped->_id);
                break;
            case 10054:
            case 10053:
                __debugbreak();
                break;

            default:
                printf("Error  SeqNumber %lld GetLastError :  %d \n", overlapped->_id,LastError);
     
            }
        }
        else
            printf("%lld Sync \n", overlapped->_id);
    }
    {
        WSABUF wsabuf;
       
        overlapped = new MyOVERLAPPED(en_SendMode::SecondSend);

        wsabuf.buf = &session->buffer[overlapped->_id];
        wsabuf.len = sizeof(BYTE);
        sendRetval = WSASend(session->m_sock, &wsabuf, 1, nullptr, 0, overlapped, nullptr);
        LastError = GetLastError();

        if (sendRetval != 0)
        {

            switch (LastError)
            {
            case WSA_IO_PENDING:
                printf("%lld WSA_IO_PENDING \n", overlapped->_id);
                break;
            case 10054:
            case 10053:
                __debugbreak();
                break;

            default:
                printf("Error  SeqNumber %lld GetLastError : %d  \n", overlapped->_id, LastError);
            }
        }
        else
            printf("%lld Sync \n", overlapped->_id);
    }
}

unsigned AcceptThread(void *arg)
{
    WSAData wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN addr;
    LINGER linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);
    InetPtonW(AF_INET, L"127.0.0.1", &addr.sin_addr);
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(LINGER));

    if (bZeroCopy)
    {
        int buflen = 0;
        setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
        printf("getsockopt SendBuffer Len : %d \n", buflen);
    }
    else
    {
        int buflen = 1000;
        setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
        printf("getsockopt SendBuffer Len : %d \n", buflen);
    }

    bind(listen_sock, (sockaddr *)&addr, sizeof(addr));

    InitializeSRWLock(&srw_list);

    hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, 0);
    listen(listen_sock, SOMAXCONN_HINT(65535));
    SOCKADDR_IN clientAddr;
    SOCKET client_sock;
    int len = sizeof(clientAddr);

    static ull id = 1;

    client_sock = accept(listen_sock, (sockaddr *)&clientAddr, &len);
    if (client_sock == INVALID_SOCKET)
        __debugbreak();
    clsSession *session = new clsSession(id++, client_sock);

    CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);

    for (ull i = 0; i < LOOP_CNT; i++)
    {
        SendPack(session);
    }
    Sleep(0);
    SetEvent(hWSASend_Overapped);

    return 0;
}
ull arridx = -1;

unsigned WorkerThread(void *arg)
{
    int LastError;
    int sendRetval;
    size_t *arrArg = reinterpret_cast<size_t *>(arg);

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    MyOVERLAPPED *myOverlapped;

    clsSession *session;

    static long long loopcnt = 0;
    static long long seqNumber = 0;

    WaitForSingleObject(hWSASend_Overapped, INFINITE);

    while (1)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        SetLastError(0);

        GetQueuedCompletionStatus(hIOCP, &transferred, &key, &overlapped, INFINITE);
        retry:
        session = reinterpret_cast<clsSession *>(key);

        myOverlapped = reinterpret_cast<MyOVERLAPPED *>(overlapped);

        if (session == nullptr)
            __debugbreak();
        printf("SeqNumber  : %lld ", seqNumber);
        if (loopcnt == 0)
        {
            if (myOverlapped->_mode != en_SendMode::FirstSend)
            {
                printf("Send Error \n");
                seqNumber++;
                loopcnt = 1;
                goto retry;
                
                __debugbreak();
            }
            else
                printf("en_SendMode::FirstSend \n");
            if (seqNumber != myOverlapped->_id)
                __debugbreak();
            loopcnt = 1;
        }
        else if (loopcnt == 1)
        {
            if (myOverlapped->_mode != en_SendMode::SecondSend)
            {
                printf("Send Error \n");
                seqNumber++;
                loopcnt = 0;
                goto retry;

                __debugbreak();
            }
            else
                printf("en_SendMode::SecondSend \n");
            if (seqNumber != myOverlapped->_id)
                __debugbreak();
            loopcnt = 0;
        }
        seqNumber++;
        //_interlockedincrement64(&seqNumber);
        if (LOOP_CNT == seqNumber)
        {
            return 0;
        }
 
    }
    return 0;
}

int main()
{

    hExitEvent = (HANDLE)CreateEvent(nullptr, 1, false, nullptr);
    hWSASend_Overapped = (HANDLE)CreateEvent(nullptr, 0, false, nullptr);

    assert(hExitEvent != 0);

    _beginthreadex(nullptr, 0, AcceptThread, nullptr, 0, nullptr);
    Sleep(1000);
    _beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

    Sleep(INFINITE);
}
