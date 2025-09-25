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

int MAX_LEN_BUFFER = 1024 * 1024;
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
*   WorkerThread는 MyOVERLAPPED의 id와  SeqNumber 의 역전 현상이 있는지 확인   *  SeqNumber 는  완료통지 Cnt임

  문제 상황 :
    WSASend가 중간에  LastError 6을 반환한 경우가 발생 함.   * 이로인해 완료통지 Cnt 보다 id가 더 큰 현상이 나올 수 있음 
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
    SOCKET sock;
 

    LastError = WSAGetLastError();
    MyOVERLAPPED *overlapped;
    {
        WSABUF big_msg_wsabuf;
        overlapped = new MyOVERLAPPED(en_SendMode::BigMessage);
        if (overlapped == nullptr)
            __debugbreak();
        big_msg_wsabuf.buf = (char *)malloc(MAX_LEN_BUFFER);
        if (big_msg_wsabuf.buf == nullptr)
            __debugbreak();

        big_msg_wsabuf.len = MAX_LEN_BUFFER;

        SetLastError(0);
        sock = session->m_sock; // 디버깅 용
        Async_sendRetval = WSASend(sock, &big_msg_wsabuf, 1, nullptr, 0, overlapped, nullptr);
        LastError = WSAGetLastError();

        if (Async_sendRetval == SOCKET_ERROR)
        {

            switch (LastError)
            {
            case WSA_IO_PENDING:
                printf("%lld WSA_IO_PENDING  Socket_Handle %lld  \n", overlapped->_id, sock);
                break;

            default:
                printf("%lld Error  BigMessage GetLastError : %d   Socket_Handle %lld \n", overlapped->_id, LastError, sock);
                //__debugbreak();
            }
        }
        else
            printf("%lld Sync Socket_Handle %lld \n", overlapped->_id, sock);
    }
    {
        WSABUF small_msg_wsabuf;

        overlapped = new MyOVERLAPPED(en_SendMode::SmallMessage);

        if (overlapped == nullptr)
            __debugbreak();

        small_msg_wsabuf.buf = (char *)malloc(sizeof(BYTE));

        if (small_msg_wsabuf.buf == nullptr)
            __debugbreak();
        small_msg_wsabuf.len = sizeof(BYTE);

        SetLastError(0);
        sock = session->m_sock; // 디버깅 용
        sendRetval = WSASend(sock, &small_msg_wsabuf, 1, nullptr, 0, overlapped, nullptr);

        LastError = WSAGetLastError();

        if (sendRetval == SOCKET_ERROR)
        {

            switch (LastError)
            {
            case WSA_IO_PENDING:
                printf("%lld WSA_IO_PENDING  Socket_Handle %lld  \n", overlapped->_id ,sock);
                break;

            default:
                printf("%lld Error  SmallMessage GetLastError : %d   Socket_Handle %lld \n", overlapped->_id, LastError, sock);
                //__debugbreak();
            }
        }
        else
            printf("%lld Sync Socket_Handle %lld \n", overlapped->_id , sock);
    }
}

unsigned AcceptThread(void *arg)
{
    // WSAStart 및 listen 초기화 listen 동작.
    int listenError;

    {
        listenError = CreateAndStartListen();
        if (listenError != 0)
        {
            printf("CreateAndStartListen Error\n");
            __debugbreak();
        }
    }

    hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, 0);
    if (hIOCP == nullptr)
    {
        printf("Create hIOCP Failed  GetLastError:  %d \n",GetLastError());
        __debugbreak();
    }

    {
        SOCKADDR_IN clientAddr;
        SOCKET client_sock;
        void* CreateIoCompletionPort_retval;

        int len = sizeof(clientAddr);

        client_sock = accept(listen_sock, (sockaddr *)&clientAddr, &len);
        if (client_sock == INVALID_SOCKET)
            __debugbreak();

        printf("Accept Success ");
        system("pause");

        clsSession *session = new clsSession(client_sock);
        if (session == nullptr)
            __debugbreak();
        CreateIoCompletionPort_retval = CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ULONG_PTR)session, 0);
        if (CreateIoCompletionPort_retval == nullptr)
        {
            printf("IOCP_Connect Failed\n");
            __debugbreak();

        }
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
        if (overlapped == nullptr)
            __debugbreak();

    retry:
        printf("seqNumber  : %lld ", seqNumber);

        // WSASend에 실패한다고 seqNumber가 id보다 커질순 없음. 순서가 지켜지지않음의 증명.
        if (seqNumber > myOverlapped->_id)
        {
            printf("Not equle  seqNumber :%lld  >  overlappedID : %lld\n", seqNumber, myOverlapped->_id);
            __debugbreak();
        }
        else if (seqNumber < myOverlapped->_id)
        {
            printf(" Did not receive IO completion message \/ send error \n");
            seqNumber++;
            goto retry; // 실패한 Overlapped ID 는 완료통지에 없음. 했다치고 seq 증가 와 Toggle 변경
        }

        switch (myOverlapped->_mode)
        {
        case en_SendMode::BigMessage:
            printf("en_SendMode::BigMessage \n");
            break;

        case en_SendMode::SmallMessage:
            printf("en_SendMode::SmallMessage \n");
            break;

        default:
            printf("Not Define en_SendModeType \n");
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

    int a;
    printf("BigMessage의 크기 : \n 1. 100Byte \n 2. 1KB \n 3. 1MB  \n 4. 4MB\n");
    scanf_s("%d", &a);
    switch (a)
    {
    case 1:
        MAX_LEN_BUFFER = 100;
        break;
    case 2:
        MAX_LEN_BUFFER = 1024;
        break;
    case 3:
        MAX_LEN_BUFFER = 1024 * 1024;
        break;
    case 4:
        MAX_LEN_BUFFER = 1024 * 1024 * 4;
        break;
    }
    
    hWSASend_Overapped = (HANDLE)CreateEvent(nullptr,1, false, nullptr);
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

    InetPtonW(AF_INET, L"0.0.0.0", &addr.sin_addr);
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET)
    {
        printf("listen_sock Create Error  %d \n", WSAGetLastError());
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
        printf("bind Error  %d\n", WSAGetLastError());
        return -1;
    }
    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
    if (listen_retval != 0)
    {
        printf("listen Error  %d\n", WSAGetLastError());
        return -1;
    }
    printf("listen Success \n");
    printf("==================================== \n");
    return 0;
}
