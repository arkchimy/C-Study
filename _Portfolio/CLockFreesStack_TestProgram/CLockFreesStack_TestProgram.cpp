// CLockFreesStack_TestProgram.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../_3Course/lib/CLockFreeStack_lib/CLockFreeStack.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
#include "../../_3Course/lib/Profiler_MultiThread/Profiler_MultiThread.h"

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
bool CompareTest(CLockFreeStack<int> &stack);

unsigned WorkerThread(void *arg)
{
    CLockFreeStack<int> *stack;
    stack = reinterpret_cast<CLockFreeStack<int> *>(arg);
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
        Profiler profile;
        {
            profile.Start(L"NormalPush");
            stack->Push(local_list.front());
            profile.End(L"NormalPush");
        }
        local_list.pop_front();
        if (rand() % 10 > 2)
        {
            outPutData = stack->Pop();
            stack->Push(outPutData);
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
    srand(3);
    hStartEvent = CreateEvent(nullptr, 1, 0, nullptr);
    hInitalizeEvent = CreateEvent(nullptr, 1, 0, nullptr);
    if (hStartEvent == nullptr || hInitalizeEvent == nullptr)
        __debugbreak();

    hThread = new HANDLE[iWorkerThreadCnt];
    bool retval;

    CLockFreeStack<int> stack;
    for (int i = 0; i < iWorkerThreadCnt; i++)
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &stack, 0, nullptr);
    SetEvent(hInitalizeEvent);

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

        if (GetAsyncKeyState('A') || GetAsyncKeyState('a'))
        {
            Profiler::SaveAsLog(L"Profiler.txt");
        }
        else if (GetAsyncKeyState('D') || GetAsyncKeyState('d'))
        {
            Profiler::Reset();
        }
    }
}

void InputData_Initalization()
{
    int size = 500;

    for (int i = 0; i < size; i++)
        inputData.push_back(rand() % 1000);
    compareData = inputData;
}

bool CompareTest(CLockFreeStack<int>& stack)
{
    int outPutData;

    while (stack.m_size != 0)
    {
        if (compareData.size() == 0)
            return false;

        outPutData = stack.Pop();
        auto iter  = std::find(compareData.begin(), compareData.end(), outPutData);
        compareData.erase(iter);

    }
    if (compareData.size() != 0)
        return false;

    return true;
}
