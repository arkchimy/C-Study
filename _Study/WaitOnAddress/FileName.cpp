
#include <iostream>

#include <Windows.h>
#include <synchapi.h>
#include <thread>

#pragma comment(lib,"Synchronization.lib")
#define LOOPCNT 10000

LONG g_num;

unsigned long long g_Value;

HANDLE hEvent;

unsigned int  WorkerThread(void *arg)
{

    WaitForSingleObject(hEvent, INFINITE);

    for (int i = 0; i < LOOPCNT; i++)
    {
        LONG num = 1;
        do
        {
            WaitOnAddress((void*)&g_num, &num, sizeof(num), INFINITE);

        } while (InterlockedExchange((LONG *)&g_num, 1) == 1);
        g_Value++;

        InterlockedExchange((LONG *)&g_num, 0);
        WakeByAddressSingle(&g_num);
    }
    return 0;
}

#define WorkerThreadCnt 4
int main()
{
    HANDLE hThread[WorkerThreadCnt];

    hEvent = CreateEvent(nullptr, 1, 0, nullptr);

    while (1)
    {

        for (int i = 0; i < WorkerThreadCnt; i++)
            hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        SetEvent(hEvent);

        WaitForMultipleObjects(WorkerThreadCnt, hThread, TRUE, INFINITE);
        for (int i = 0; i < WorkerThreadCnt; i++)
            CloseHandle(hThread[i]);
      
        ResetEvent(hEvent);

        printf("g_Value %lld  \n", g_Value);
        if (g_Value % LOOPCNT != 0)
            __debugbreak();
    
    }
    return 0;
}