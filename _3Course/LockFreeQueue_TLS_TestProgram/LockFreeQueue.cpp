#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/lib/CSystemLog_lib/CSystemLog_lib.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"

#include "../../_4Course/_lib/CTlsLockFreeQueue/CTlsLockFreeQueue.h"
#include "../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"

#include <iostream>
#include <list>
#include <thread>

std::list<int> inputData;
std::list<int> compareData;

SRWLOCK srw_inputDataLock;
SRWLOCK srw_compareDataLock;
HANDLE hStartEvent;
HANDLE hInitalizeEvent;

LONG64 readyThreadChkCount;

CDump dump;

void InputData_Initalization();

bool CompareTest(CTlsLockFreeQueue<int> &stack);

unsigned WorkerThread(void *arg)
{
    CTlsLockFreeQueue<int> *queue;
    queue = reinterpret_cast<CTlsLockFreeQueue<int> *>(arg);
    std::list<int> local_list;
retry:
    WaitForSingleObject(hInitalizeEvent, INFINITE);

    while (1)
    {
        AcquireSRWLockExclusive(&srw_inputDataLock);
        if (inputData.empty() == true)
        {
            printf("ThreadID : %d  local_list size : %lld \n", GetCurrentThreadId(), local_list.size());
            ReleaseSRWLockExclusive(&srw_inputDataLock);
            break;
        }
        local_list.push_back(inputData.front());
        inputData.pop_front();

        ReleaseSRWLockExclusive(&srw_inputDataLock);
        Sleep(0);
    }
    _interlockedincrement64(&readyThreadChkCount);

    WaitForSingleObject(hStartEvent, INFINITE);

    int outPutData;
    while (local_list.empty() == false)
    {
        queue->Push(local_list.front());
        local_list.pop_front();
        if (rand() % 10 > 2)
        {
            queue->Pop(outPutData);
            queue->Push(outPutData);
        }
    }
    _interlockedincrement64(&readyThreadChkCount);
    WaitForSingleObject(hInitalizeEvent, INFINITE);

    goto retry;

    return 0;
}

int main()
{
    int iWorkerThreadCnt;
    HANDLE *hThread;

    InitializeSRWLock(&srw_inputDataLock);
    InitializeSRWLock(&srw_compareDataLock);

    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"iWorkerThreadCnt", iWorkerThreadCnt);
    }
    CSystemLog::GetInstance()->SetDirectory(L"SystemLog");
    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_Mode);

    srand(3);
    hStartEvent = CreateEvent(nullptr, 1, 0, nullptr);
    hInitalizeEvent = CreateEvent(nullptr, 1, 0, nullptr);
    if (hStartEvent == nullptr || hInitalizeEvent == nullptr)
        __debugbreak();

    hThread = new HANDLE[iWorkerThreadCnt];
    if (hThread == nullptr)
        __debugbreak();

    bool retval;

    CTlsLockFreeQueue<int> stack;
    for (int i = 0; i < iWorkerThreadCnt; i++)
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &stack, 0, nullptr);

    while (1)
    {

        InputData_Initalization();
        SetEvent(hInitalizeEvent);

        printf("ToTal list size : %lld \n", inputData.size());

        while (InterlockedCompareExchange((ull *)&readyThreadChkCount, 0, iWorkerThreadCnt) != iWorkerThreadCnt)
        {
            YieldProcessor();
        }
        ResetEvent(hInitalizeEvent);
        SetEvent(hStartEvent);

        while (InterlockedCompareExchange((ull *)&readyThreadChkCount, 0, iWorkerThreadCnt) != iWorkerThreadCnt)
        {
            YieldProcessor();
        }

        ResetEvent(hStartEvent);

        retval = CompareTest(stack);
        if (retval == false)
            __debugbreak();
        if (GetAsyncKeyState(VK_UP))
        {
            printf(" ========================== DEBUG_MODE =========================================== \n");
            CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_Mode);
        }
        else if (GetAsyncKeyState(VK_DOWN))
        {
            printf(" ========================== SYSTEM_Mode =========================================== \n");
   
            CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::SYSTEM_Mode);
        }
        if (CSystemLog::GetInstance()->m_Log_Level == en_LOG_LEVEL::DEBUG_Mode)
            system("pause");
    }
}

void InputData_Initalization()
{
    int size = tlsPool_init_Capacity;

    for (int i = 0; i < size; i++)
        inputData.push_back(rand() % 1000);
    compareData = inputData;
}

bool CompareTest(CTlsLockFreeQueue<int> &queue)
{
    int outPutData;

    while (queue.m_size != 0)
    {
        if (compareData.size() == 0)
            return false;

        queue.Pop(outPutData);
        auto iter = std::find(compareData.begin(), compareData.end(), outPutData);
        compareData.erase(iter);
    }
    if (compareData.size() != 0)
        return false;

    return true;
}