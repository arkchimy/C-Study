#include "../lib/MT_CRingBuffer_lib/framework.h"
#include <WS2tcpip.h>
#include <WinSock2.h>

#include <Windows.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

struct st_Header
{
    DWORD type;
    DWORD len;
};
enum class enOverlappedMode
{
    Recv,
    Send,
    MAX,
};

struct st_MyOverlapped : public OVERLAPPED
{
  public:
    st_MyOverlapped(enOverlappedMode mode)
        : _mode(mode)
    {
    }
    CRingBuffer _buffer;
    enOverlappedMode _mode;
};
class clsSession
{
  public:
    SOCKET _sock;
    st_MyOverlapped Sendoverlapped = st_MyOverlapped(enOverlappedMode::Send);
    st_MyOverlapped Recvoverlapped = st_MyOverlapped(enOverlappedMode::Recv);
    unsigned long long io_Count;
};

HANDLE hNetworkIOCP; // IOCP Completion Port
DWORD LogicalProcessCount();
void RecvProcedure(clsSession *session, DWORD transffered);

unsigned WorkerThread(void *arg)
{
    DWORD transffer;
    clsSession *session = nullptr;
    ULONG_PTR key;
    OVERLAPPED *overlapped;

    GetQueuedCompletionStatus(hNetworkIOCP, &transffer, &key, &overlapped, INFINITE);
    session = reinterpret_cast<clsSession *>(key);

    if (overlapped == nullptr || session == nullptr)
        return -1;
    switch (static_cast<st_MyOverlapped *>(overlapped)->_mode)
    {
    case enOverlappedMode::Recv:
        RecvProcedure(session, transffer);
        break;
    }

    if (InterlockedDecrement(&session->io_Count) == 0)
        closesocket(session->_sock);

    printf("Thread ID : %d  Return \n", GetCurrentThreadId());
    return 0;
}
unsigned AcceptThread(void *arg) 
{
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    SOCKET client_sock;
    SOCKADDR_IN addr;
    SOCKADDR_IN ClientAddr;

    int socklen = sizeof(ClientAddr);
    addr.sin_port = htons(6000);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);


    bind(listen_sock, (sockaddr *)&addr, sizeof(addr));
    listen(listen_sock, SOMAXCONN_HINT(65535));


    while (1)
    {
        client_sock = accept(listen_sock, (sockaddr *)&ClientAddr, &socklen);
        if (client_sock == INVALID_SOCKET)
            return;

    }
    return 0;
}
int main()
{

    DWORD logicalProcess;
    HANDLE *hThread;
    int StartupRetval;

    WSAData wsa;
    StartupRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (StartupRetval != 0)
        return -1;

    HANDLE hAcceptThread = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, nullptr, 0, nullptr);

    logicalProcess = LogicalProcessCount();
    hNetworkIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, logicalProcess - 1);
    __assume(hNetworkIOCP != nullptr);

    hThread = new HANDLE[logicalProcess * 2];

    for (DWORD i = 0; i < logicalProcess * 2; i++)
    {
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        CreateIoCompletionPort(hNetworkIOCP, hThread[i], 0, 0);
    }

    WaitForMultipleObjects(logicalProcess * 2, hThread, true, INFINITE);


    WSACleanup();
}

// LogiclaProcess 갯수 가져오기.
DWORD LogicalProcessCount()
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *infos = nullptr;
    DWORD len = 0;
    GetLogicalProcessorInformation(infos, &len);

    infos = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(len);
    if (infos == nullptr)
        __debugbreak();
    GetLogicalProcessorInformation(infos, &len);

    DWORD cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); // 반복문
                                                                    // --- 여기서부터 논리프로세서 개수 계산 추가 ---
    DWORD logicalProcessorCount = 0;
    for (DWORD i = 0; i < cnt; ++i)
    {
        if (infos[i].Relationship == RelationProcessorCore)
        {
            // ProcessorMask의 비트 수 세기 (set된 bit=논리 프로세서 수)
            ULONG_PTR mask = infos[i].ProcessorMask;
            while (mask)
            {
                logicalProcessorCount += mask & 1;
                mask >>= 1;
            }
        }
    }
    printf("logicalProcessorCount : %d \n", logicalProcessorCount);

    return logicalProcessorCount;
}

void RecvProcedure(clsSession *session, DWORD transffered)
{
}
