#include <iostream>

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <thread>


#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
#include "../../error_log.h"

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

#include <vector>

struct stSession
{
    int id;
    OVERLAPPED recvOverlapped;
    OVERLAPPED sendOverlapped;

    CRingBuffer sendBuffer;
    CRingBuffer recvBuffer;
};
BOOL GetLogicalProcess(DWORD &out);
using POVERLAPPED = LPOVERLAPPED;



unsigned WorkerThread(void *arg) 
{
    HANDLE hPort = (HANDLE)arg;
    DWORD transferred;
    stSession* session;
    POVERLAPPED overlapped;

    while (1)
    {
        session = nullptr;
        transferred = 0;
        overlapped = nullptr;

        GetQueuedCompletionStatus(hPort, &transferred, (PULONG_PTR)&session, &overlapped, INFINITE);
        if (overlapped == nullptr)
        {
            return -1;
        }
        
        if (overlapped == &session->recvOverlapped)
        {
            RecvPost();
        }
        else if (overlapped == &session->sendOverlapped)
        {
            //SendProc
        }


    }
    return 0;
}

int main()
{

    int clientCnt;

    LINGER linger;
    int disconnect;
    int bZeroCopy;
    int WorkerThreadCnt;
    int reduceThreadCount;
    DWORD Logical_Cnt = 0;
    HANDLE hPort;
    HANDLE *hWorkerThread;

    linger.l_linger = 0;

    {
        Parser parser;
        if (parser.LoadFile(L"EchoClient_Config.txt") == false)
            ERROR_FILE_LOG(L"ParserError.txt", L"LoadFile");

        parser.GetValue(L"ClientCnt", clientCnt);
        parser.GetValue(L"Disconnect", disconnect);
        parser.GetValue(L"LingerOn", linger.l_onoff);
        parser.GetValue(L"ZeroCopy", bZeroCopy);
        parser.GetValue(L"WorkerThreadCnt", WorkerThreadCnt);
        parser.GetValue(L"ReduceThreadCount", reduceThreadCount);
    }
    //TODO : CriticalError 에 대한 처리 
    if (GetLogicalProcess(Logical_Cnt) == false)
        ERROR_FILE_LOG(L"Critical_Error.txt", L"GetLogicalProcess");

    hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, Logical_Cnt - reduceThreadCount);
    printf("Logical_Cnt - DecreamentConcurrent : \t %d - %d = %d \n", Logical_Cnt, reduceThreadCount, Logical_Cnt - reduceThreadCount);

    hWorkerThread = new HANDLE[WorkerThreadCnt];

    for (int i = 0; i < WorkerThreadCnt; i++)
        hWorkerThread[i] = (HANDLE)_beginthreadex(nullptr, 0, &WorkerThread, hPort, 0, nullptr);
    // TODO : ramda 띵킹해
}

BOOL GetLogicalProcess(DWORD &out)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *infos = nullptr;
    DWORD len;
    DWORD cnt;
    DWORD temp = 0;

    len = 0;

    GetLogicalProcessorInformation(infos, &len);

    infos = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];

    if (infos == nullptr)
        return false;

    GetLogicalProcessorInformation(infos, &len);

    cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); // 반복문

    for (int i = 0; i < cnt; i++)
    {
        if (infos[i].Relationship == RelationProcessorCore)
        {
            DWORD mask = infos[i].ProcessorMask;
            // 논리 프로세서의 수는 set된 비트의 개수
            while (mask)
            {
                temp += (mask & 1);
                mask >>= 1;
            }
        }
    }
    printf("LogicalProcess Cnt : %d \n", temp);

    delete[] infos;
    out = temp;
    return true;
}
