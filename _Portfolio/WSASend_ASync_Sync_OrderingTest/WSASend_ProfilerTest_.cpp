#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>

#include <assert.h>
#include <thread>

#pragma comment(lib, "ws2_32")

using ull = unsigned long long;
using ll = long long;

SOCKET listen_sock;

HANDLE hIOCP;
HANDLE hWSASend_Overapped; // WSASend를 100회 다 떄리면 SetEvent

int bZeroCopy = false;

#define MAX_LEN_BUFFER 1024 * 1024 * 2 // BigMessage 의 크기
#define LOOP_CNT 50                    // 반복 횟수

/*
* 실험 목표 : ASync와 Sync의 순서가 맞는가?
* 구조 :
*      AcceptThread x 1 ,  WorkerThread x 1 , main
* 설명 :
*   
*   AcceptThread가  1개의 Session을 받으면  총 LOOP_CNT * 2 만큼 WSASend 한다.
*   MyOVERLAPPED은  생성자마다 id 값을 갖는다.  // 완료통지Cnt (SeqNumber) 와 비교 변수.
* 
*   for ( 0 < LOOP_CNT )
*       BigMessage   WSASend 후    // 목적 : ASync 유도
*       SmallMessage  WSASend 
*   
*   모든 I/O요청을 한 후 Event객체를 이용하여 WorkerThread를 깨움.
*   WorkerThread는 MyOVERLAPPED를 확인하여 Togle 변수를 이용하여 BigMessage 과 SmallMessage 이 번갈아 오는지 확인.

  문제 상황 :
    WSASend가 중간에  LastError 6을 반환한 경우가 발생 함. 
*/
enum class en_SendMode
{
    BigMessage,
    SmallMessage,
    MAX,
};
struct MyOVERLAPPED : public OVERLAPPED
{
    MyOVERLAPPED(en_SendMode mode) : _mode(mode)
    {
        _id = s_id++;
    }

    ll _id = 0;
    inline static ll s_id = 0;
    en_SendMode _mode;
};

struct clsSession
{
    clsSession(SOCKET sock)
        : m_sock(sock)
    {
    }
    SOCKET m_sock;
};

int CreateAndStartListen();

void SendPack(clsSession *session)
{
    int sendRetval, LastError;
    int Async_sendRetval, Async_LastError;

    LastError = GetLastError();
    MyOVERLAPPED *overlapped;
    {
        WSABUF wsabuf;
        overlapped = new MyOVERLAPPED(en_SendMode::BigMessage);

        wsabuf.buf = (char *)malloc(MAX_LEN_BUFFER);
        if (wsabuf.buf == nullptr)
            __debugbreak();

        memset(wsabuf.buf, overlapped->_id, MAX_LEN_BUFFER);
        wsabuf.len = MAX_LEN_BUFFER;

        SetLastError(0);
        Async_sendRetval = WSASend(session->m_sock, &wsabuf, 1, nullptr, 0, overlapped, nullptr);
        LastError = GetLastError();

        if (Async_sendRetval != 0)
        {

            switch (LastError)
            {
            case WSA_IO_PENDING:
                printf("%lld WSA_IO_PENDING \n", overlapped->_id);
                break;

            default:
                printf("Error  SeqNumber %lld GetLastError :  %d \n", overlapped->_id, LastError);
                //__debugbreak();
            }
        }
        else
            printf("%lld Sync \n", overlapped->_id);
    }
    {
        WSABUF wsabuf;

        overlapped = new MyOVERLAPPED(en_SendMode::SmallMessage);

        wsabuf.buf = (char *)malloc(sizeof(BYTE));
        wsabuf.len = sizeof(BYTE);

        SetLastError(0);
        sendRetval = WSASend(session->m_sock, &wsabuf, 1, nullptr, 0, overlapped, nullptr);

        LastError = GetLastError();

        if (sendRetval != 0)
        {

            switch (LastError)
            {
            case WSA_IO_PENDING:
                printf("%lld WSA_IO_PENDING \n", overlapped->_id);
                break;

            default:
                printf("Error  SeqNumber %lld GetLastError : %d  \n", overlapped->_id, LastError);
                //__debugbreak();
            }
        }
        else
            printf("%lld Sync \n", overlapped->_id);
    }
}

unsigned AcceptThread(void *arg)
{
    // WSAStart 및 listen 초기화 listen 동작.
    int listenError;

    {
        listenError = CreateAndStartListen();
        if (listenError != 0)
            printf("CreateAndStartListen Error\n");
    }

    {
        SOCKADDR_IN clientAddr;
        SOCKET client_sock;

        int len = sizeof(clientAddr);

        client_sock = accept(listen_sock, (sockaddr *)&clientAddr, &len);
        if (client_sock == INVALID_SOCKET)
            __debugbreak();
        clsSession *session = new clsSession(client_sock);

        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ULONG_PTR)session, 0);

        for (ull i = 0; i < LOOP_CNT; i++)
        {
            SendPack(session);
        }
    }
    SetEvent(hWSASend_Overapped);

    return 0; // 한명만 받고 return 함.  closesocket 은 없음
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

    ll Toggle = 0;    // BigMessage  와 SmallMessage의 번갈음을  표기
    ll seqNumber = 0; // 완료통지를 뺄때마다 1씩 증가. 

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

        session = reinterpret_cast<clsSession *>(key);

        myOverlapped = reinterpret_cast<MyOVERLAPPED *>(overlapped);

        if (session == nullptr)
            __debugbreak();

    retry:
        printf("seqNumber  : %lld ", seqNumber);

        // WSASend에 실패한다고 seqNumber가 id보다 커질순 없음. 순서가 지켜지지않음의 증명.
        if (seqNumber > myOverlapped->_id)
        {
            printf("Not equle  seqNumber :%lld  >  overlappedID : %lld\n", seqNumber, myOverlapped->_id);
            __debugbreak();
        }

        switch (myOverlapped->_mode)
        {
        case en_SendMode::BigMessage:
            if (Toggle != 0)
            {
                printf(" Did not receive IO completion message \/ send error \n");
                seqNumber++;
                Toggle = 0;
                goto retry; // 실패한 Overlapped ID 는 완료통지에 없음. 했다치고 seq 증가 와 Toggle 변경
            }
            printf("en_SendMode::BigMessage \n");
            Toggle = 1;
            break;

        case en_SendMode::SmallMessage:
            if (Toggle != 1)
            {
                printf(" Did not receive IO completion message \/ send error \n");
                seqNumber++;
                Toggle = 1;
                goto retry; // 실패한 Overlapped ID 는 완료통지에 없음.
            }
            printf("en_SendMode::SmallMessage \n");
            Toggle = 0;
            break;

        default:
            printf("Not Define en_SendModeType \n");
            __debugbreak();
        }
        if (seqNumber != myOverlapped->_id)
        {
            printf(" seqNumber :%lld  !=  overlappedID : %lld\n", seqNumber, myOverlapped->_id);
            __debugbreak();
        }

        seqNumber++;

        if (LOOP_CNT * 2 == seqNumber)
        {
            printf("==================================== \n");
            printf("모든 메세지를 처리 함.\n");
            return 0;
        }
    }
    return 0;
}

int main()
{

    hWSASend_Overapped = (HANDLE)CreateEvent(nullptr, 0, false, nullptr);
    assert(hWSASend_Overapped != 0);

    _beginthreadex(nullptr, 0, AcceptThread, nullptr, 0, nullptr);
    _beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

    Sleep(INFINITE);
}

int CreateAndStartListen()
{
    WSAData wsadata;
    int wsastart_retval, bind_retval, listen_retval;

    SOCKADDR_IN addr;
    LINGER linger;

    wsastart_retval = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (wsastart_retval != 0)
    {
        printf("WSAStartup Error %d", wsastart_retval);
        return -1;
    }

    linger.l_onoff = 1;
    linger.l_linger = 0;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);

    InetPtonW(AF_INET, L"127.0.0.1", &addr.sin_addr);
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
    {
        printf("listen_sock Create Error  %d \n", GetLastError());
        return -1;
    }

    setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(LINGER));

    if (bZeroCopy)
    {
        int buflen = 0;
        setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
    }
    else
    {
        int buflen = 1000;
        setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
    }

    bind_retval = bind(listen_sock, (sockaddr *)&addr, sizeof(addr));
    if (bind_retval != 0)
    {
        printf("bind Error  %d\n", GetLastError());
        return -1;
    }

    hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, 0);
    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
    if (listen_retval != 0)
    {
        printf("listen Error  %d\n", GetLastError());
        return -1;
    }
    printf("listen Success \n");
    printf("==================================== \n");
    return 0;
}
