#include <iostream>
#include <list>
#include <string>
#include <thread>
#include <windows.h>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../lib/MT_ringBuffer_lib/CRingBuffer.h">

#define NULLCHECK_CREATE_EVENT(x)                   \
    {                                               \
        x = CreateEvent(nullptr, true, 0, nullptr); \
        if (x == nullptr)                           \
        {                                           \
            printf("CreateEvent Fail \n");          \
            __debugbreak();                         \
        }                                           \
    }

#define dfJOB_ADD 0
#define dfJOB_DEL 1
#define dfJOB_SORT 2
#define dfJOB_FIND 3
#define dfJOB_PRINT 4 //<< 출력 or 저장 / 읽기만 하는 느린 행동
#define dfJOB_QUIT 5
#define dfJOB_MAX 6

struct st_MSG_HEAD
{
    short shType;
    short shPayLoadLen;
};

HANDLE hExitEvent, hStartEvent;
int workerCnt = 0;

CRingBuffer ringBuffer(30000);
std::list<std::wstring> g_List;

unsigned WorkerThread(void *arg);
void Initalize(HANDLE **hWorkerThread);
void CreateMsg();

int main()
{
    HANDLE* hWorkerThread;

    DWORD frameInterval, retVal;

    Initalize(&hWorkerThread); // workerCnt 갯수 가져옴.

    NULLCHECK_CREATE_EVENT(hExitEvent);
    NULLCHECK_CREATE_EVENT(hStartEvent);

    frameInterval = 50;

    Sleep(1000);
    SetEvent(hStartEvent);

    srand(1);

    while (1)
    {
        retVal = WaitForSingleObject(hExitEvent, frameInterval);
        if (retVal == WAIT_OBJECT_0)
            break;
        CreateMsg();
    }
    retVal = WaitForMultipleObjects(workerCnt, hWorkerThread, true, 1000);
    if (retVal == WAIT_TIMEOUT)
    {
        std::cout << "비정상 종료 \n";
        return -1;
    }
    std::cout << "정상 종료 \n";

    return 0;
}

unsigned WorkerThread(void *arg)
{

    WaitForSingleObject(hStartEvent, INFINITE);
    printf("WorkerThread ID :  %d  StartEvent \n", GetCurrentThreadId());

    while (1)
    {
    }

    return 0;
}

void Initalize(HANDLE **pphWorkerThread)
{
    Parser parser;
    if (parser.LoadFile(L"Config.txt") == false)
        printf("File_OpenFail\n");
    parser.GetValue(L"WorkerThreadCnt", workerCnt);

    *pphWorkerThread = new HANDLE[workerCnt];

    for (int i = 0; i < workerCnt; i++)
    {
 
        (*pphWorkerThread)[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        if ((*pphWorkerThread)[i] == nullptr)
        {
            printf(" Thread Cnt %d  CreateFail\n", i);
            __debugbreak();
        }
    }
}

void CreateMsg()
{
    static const std::wstring chars = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static const int strlen = lstrlenW(chars.c_str());

    st_MSG_HEAD head;

    head.shType = rand() % dfJOB_MAX;
    head.shPayLoadLen = rand() % strlen;

    
    switch (head.shType)
    {
    case dfJOB_ADD:
        break;
    case dfJOB_DEL:
        break;
    case dfJOB_SORT:
        break;
    case dfJOB_FIND:
        break;
    case dfJOB_PRINT:
        break;
    case dfJOB_QUIT:
        break;
    }
}
