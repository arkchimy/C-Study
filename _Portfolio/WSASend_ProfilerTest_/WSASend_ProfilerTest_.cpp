// WSASend_ProfilerTest_.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>
#include <thread>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include <strsafe.h>

#include <list>
#include 	<assert.h>
#include "CLargePage.h"

#pragma comment(lib, "ws2_32")

using ull = unsigned long long;

ull *TPSarr;
SOCKET listen_sock;
std::list<SOCKET> list_sock;

SRWLOCK srw_list;
HANDLE hIOCP;
HANDLE hExitEvent;

int bZeroCopy;
int BUFFER_LEN_MAX = 1000;
int WSABUF_CNT = 1;
int LargePage;



struct clsSession
{
    clsSession(ull id, SOCKET sock)
        : sessionID(id), m_sock(sock)
    {
    }
    ull sessionID;
    SOCKET m_sock;
    OVERLAPPED overlapped;

    char *buffer = CLargePage::GetInstance().Alloc(BUFFER_LEN_MAX,LargePage);
    WSABUF wsabuf[100];
};
void SendPack(clsSession *session)
{
    int sendRetval, LastError;
    DWORD flag = 0;
    for (int i = 0; i < WSABUF_CNT; i++)
    {
        session->wsabuf[i].len = BUFFER_LEN_MAX / WSABUF_CNT * (i + 1);
        if (i == 0)
            session->wsabuf[i].buf = session->buffer;
        else
            session->wsabuf[i].buf = session->buffer + session->wsabuf[i].len;
    }
    ZeroMemory(&session->overlapped, sizeof(OVERLAPPED));
    if (bZeroCopy == false)
    {
        Profiler profile;
        profile.Start(L"WSASend");
        sendRetval = WSASend(session->m_sock, &session->wsabuf[0], WSABUF_CNT, nullptr, flag, &session->overlapped, nullptr);
        LastError = GetLastError();
        profile.End(L"WSASend");
    }
    else
    {
        Profiler profile;
        profile.Start(L"bZeroCopy_WSASend");
        sendRetval = WSASend(session->m_sock, &session->wsabuf[0], WSABUF_CNT, nullptr, flag, &session->overlapped, nullptr);
        LastError = GetLastError();
        profile.End(L"bZeroCopy_WSASend");
    }
    if (sendRetval != 0)
    {
        
        switch (LastError)
        {
        case WSA_IO_PENDING:
            break;
        case 10054:
        case 10053:
            break;

        default:
            closesocket(session->m_sock);
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

    if (bZeroCopy)
    {
        int buflen = 0;
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

    while (1)
    {
        static ull id = 1;

        client_sock = accept(listen_sock, (sockaddr *)&clientAddr, &len);
        if (client_sock == INVALID_SOCKET)
            __debugbreak();
        clsSession *session = new clsSession(id++, client_sock);

        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)session, 0);
        

        SendPack(session);
    }

    return 0;
}
ull arridx = -1;

unsigned WorkerThread(void *arg)
{
    int TPSid = InterlockedIncrement(&arridx);

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
        TPSarr[TPSid]++;

    }
    return 0;
}

int WokerThreadCnt = 0;

unsigned MonitorThread(void *arg) 
{
    DWORD retWait;
    ull totalTPS;
    ull currentTPS;

    double avgTPS;

    totalTPS = 0;
  
    while (1)
    {
        static double cnt = 0;
        cnt++;
        currentTPS = 0;
        retWait = WaitForSingleObject(hExitEvent, 1000);
        printf("ZeroCopy :  %d \n" , bZeroCopy);
        printf("WokerThreadCnt : %d \n", WokerThreadCnt);
        printf("BUFFER_LEN_MAX : %d \n", BUFFER_LEN_MAX);
        printf("WSABUF_CNT : %d \n", WSABUF_CNT);
        printf("LargePage : %d \n", LargePage);

        if (WAIT_OBJECT_0 != retWait)
        {
            printf("======================= \n");
            for (int i = 0; i < WokerThreadCnt; i++)
            {
                printf("%lld :  TPS \n", TPSarr[i]);
                currentTPS += TPSarr[i];
                InterlockedExchange(&TPSarr[i], 0);
            }
            if (currentTPS == 0)
            {
                cnt--;
                continue;
            }
            totalTPS += currentTPS;
            avgTPS = totalTPS / cnt;

            printf("totalTPS : %llf:  TPS \n", avgTPS);
            printf("======================= \n");
        }
        else
            break;
    }

    return 0;
}
int main()
{


    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"ZeroCopy", bZeroCopy);
        parser.GetValue(L"WokerThreadCnt", WokerThreadCnt);
        parser.GetValue(L"BUFFER_LEN_MAX", BUFFER_LEN_MAX);
        parser.GetValue(L"WSABUF_CNT", WSABUF_CNT);
        parser.GetValue(L"LargePage", LargePage);
    }
    TPSarr = new ull[WokerThreadCnt];
    ZeroMemory(TPSarr, sizeof(ull) * WokerThreadCnt);

    hExitEvent = (HANDLE)CreateEvent(nullptr, 1, false, nullptr);
    assert(hExitEvent != 0);

    _beginthreadex(nullptr, 0, AcceptThread, nullptr, 0, nullptr);
    
    Sleep(1000);
    for (int i = 0; i < WokerThreadCnt; i++)
        _beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

    _beginthreadex(nullptr, 0, MonitorThread, nullptr, 0, nullptr);

    wchar_t fileName[200];

    while (1)
    {
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            SetEvent(hExitEvent);
        }
        else if (GetAsyncKeyState('a') || GetAsyncKeyState('A'))
        {
            StringCchPrintfW(fileName, sizeof(fileName) / sizeof(wchar_t), L"%hs_Profile_BufferLen%d_ZeroCopy%d_BufferCnt%d.txt", __DATE__, BUFFER_LEN_MAX, bZeroCopy, WSABUF_CNT);
            Profiler::SaveAsLog(fileName);

        }
        else if (GetAsyncKeyState('d') || GetAsyncKeyState('D'))
        {
            Profiler::Reset();
        }
    }
}
