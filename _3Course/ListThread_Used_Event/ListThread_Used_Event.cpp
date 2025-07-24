
#include <Windows.h>
#include <iostream>
#include <list>
#include <thread>

#include <conio.h>
#include "../../_1Course/lib/Parser_lib/Parser_lib.h"

#pragma comment(lib, "winmm")
// #include <memory>
std::list<int> list;

HANDLE hSaveEvent[2];
HANDLE hExitEvent;

SRWLOCK srw_list;


void WakeSaveThread();
void ExitEvent();

unsigned PrintThread(void *arg);
unsigned DeleteThread(void *arg);
unsigned WorkerThread(void *arg);
unsigned SaveThread(void *arg);

// 콘솔창이 있으면 디버깅에 유리합니다.
int main()
{
    timeBeginPeriod(0);

    HANDLE hPrintThread;
    HANDLE hDeleteThread;
    HANDLE hWorkerThread;
    HANDLE hSaveThread;

    int iThreadCnt;
    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"ThreadCnt", iThreadCnt);
    }

    HANDLE *hWorkerThreads = new HANDLE[iThreadCnt];

    hSaveEvent[0] = CreateEvent(nullptr, 0, false, nullptr);
    hExitEvent = CreateEvent(nullptr, 1, false, nullptr);
    hSaveEvent[1] = hExitEvent;

    hPrintThread = (HANDLE)_beginthreadex(nullptr, 0, PrintThread, nullptr, 0, nullptr);
    hDeleteThread = (HANDLE)_beginthreadex(nullptr, 0, DeleteThread, nullptr, 0, nullptr);

    hSaveThread = (HANDLE)_beginthreadex(nullptr, 0, SaveThread, nullptr, 0, nullptr);

    for (int i = 0; i < iThreadCnt; i++)
    {
        hWorkerThreads[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    }

    InitializeSRWLock(&srw_list);

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            switch (ch)
            {
            case 'S':
            case 's':
                WakeSaveThread();
                break;
            case VK_ESCAPE:
                ExitEvent();
                WaitForMultipleObjects(3, hWorkerThreads, true, INFINITE);
                WaitForSingleObject(hPrintThread, INFINITE);
                WaitForSingleObject(hDeleteThread, INFINITE);
                WaitForSingleObject(hSaveThread, INFINITE);
                printf("Mainthread return 0\n");
                return 0;
            }
        }
    }
    __debugbreak();
    return -1;
}




void WakeSaveThread()
{
    SetEvent(hSaveEvent[0]);
}

void ExitEvent()
{
    SetEvent(hExitEvent);
}

unsigned PrintThread(void *arg)
{
    DWORD retval;

    while (1)
    {
        retval = WaitForSingleObject(hExitEvent, 1000);
        if (WAIT_OBJECT_0 == retval)
            break;
        AcquireSRWLockShared(&srw_list);

        for (auto data : list)
        {
            printf("%d , ", data);
        }
        printf("\n");

        ReleaseSRWLockShared(&srw_list);
   
    }
    printf("Printthread return 0\n");
    return 0;
}
unsigned DeleteThread(void *arg)
{
    DWORD retval;

    while (1)
    {
        retval = WaitForSingleObject(hExitEvent, 100);
        if (WAIT_OBJECT_0 == retval)
            break;
        AcquireSRWLockExclusive(&srw_list);
        if (list.empty() == false)
            list.pop_back();
        
        ReleaseSRWLockExclusive(&srw_list);
    }
    printf("Deletethread return 0\n");
    return 0;
}

unsigned WorkerThread(void *arg)
{
    DWORD retval;

    srand(GetCurrentThreadId());

    while (1)
    {
        retval = WaitForSingleObject(hExitEvent, 300);
        if (WAIT_OBJECT_0 == retval)
            break;
        AcquireSRWLockExclusive(&srw_list);

        list.push_back(rand() % 100);

        ReleaseSRWLockExclusive(&srw_list);
    }
    printf("Workerthread return 0\n");
    return 0;
}

unsigned SaveThread(void *arg)
{
    DWORD retval;
    while (1)
    {
        retval = WaitForMultipleObjects(2, hSaveEvent, false, INFINITE);
        if (retval == WAIT_OBJECT_0)
        {
            printf("SaveStart\n");
            AcquireSRWLockExclusive(&srw_list);
            std::list<int> temp;
            for (auto data : list)
                temp.push_back(data);

            ReleaseSRWLockExclusive(&srw_list);
            Sleep(1000);
        }
        else
            break;
    }
    printf("Savethread return 0\n");
    return 0;
}
