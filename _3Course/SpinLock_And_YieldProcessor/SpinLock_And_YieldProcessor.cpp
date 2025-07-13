

#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "winmm.lib")

void SpinLock();
void SpinUnLock();

void SpinLock_Yield_Lock();
void SpinLock_Yield_UnLock();

void SpinLock_LoopYield_Lock();
void SpinLock_LoopYield_UnLock();

void CriticaSectionlLock();
void CriticalSectionUnLock();

#define THREADCNT 4
#define TARGETCNT 10000000
#define LOOPCNT 100

alignas(64) long lSpinlock;

HANDLE hStartEvent;
HANDLE hEndEvent;
LARGE_INTEGER startTime, endTime, total, Freq;

alignas(64) long g_num;

unsigned SpinLockThread(void *arg);
unsigned SpinLockYeildThread(void *arg);
unsigned SpinLockLoopYeildThread(void *arg);

void MyCreateThread(unsigned (*type)(void *))
{

    HANDLE hThread[THREADCNT];
    QueryPerformanceFrequency(&Freq);

    for (int i = 0; i < THREADCNT; i++)
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, type, nullptr, 0, nullptr);

    Sleep(300);

    g_num = 0;
    QueryPerformanceCounter(&startTime);

    SetEvent(hStartEvent);
    WaitForSingleObject(hEndEvent, INFINITE);

    QueryPerformanceCounter(&endTime);

    WaitForMultipleObjects(THREADCNT, hThread, true, INFINITE);
    ResetEvent(hStartEvent);
    ResetEvent(hEndEvent);
}

int main()
{
    timeBeginPeriod(1);

    hStartEvent = CreateEvent(nullptr, 1, false, nullptr);
    hEndEvent = CreateEvent(nullptr, 1, false, nullptr);

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            LARGE_INTEGER total;
            total.QuadPart = 0;
            switch (ch)
            {
            case '1':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockThread);
                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" SpinLockThread time %20.3lf us \n", total.QuadPart * (1e6 / Freq.QuadPart) );
                break;
            case '2':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockYeildThread);
                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" SpinLockYeildThread time %20.3lf us \n", total.QuadPart * (1e6 / Freq.QuadPart) );
                break;
            case '3':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockLoopYeildThread);
                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" SpinLockLoopYeildThread time %20.3lf us \n", total.QuadPart * (1e6 / Freq.QuadPart) );
                break;

            case '0':
                timeEndPeriod(1);
                return 0;
            }
        }
    }
}

void SpinLock()
{

    while (_InterlockedExchange(&lSpinlock, 1) == 1)
    {
        // YieldProcessor();
    }
}

void SpinUnLock()
{
    _InterlockedExchange(&lSpinlock, 0);
}

void SpinLock_Yield_Lock()
{
    while (_InterlockedExchange(&lSpinlock, 1) == 1)
    {
        YieldProcessor();
    }
}

void SpinLock_Yield_UnLock()
{
    _InterlockedExchange(&lSpinlock, 0);
}

void SpinLock_LoopYield_Lock()
{
    while (_InterlockedExchange(&lSpinlock, 1) == 1)
    {
        for (int i = 0; i < 1024; i++)
            YieldProcessor();
    }
}

void SpinLock_LoopYield_UnLock()
{
    _InterlockedExchange(&lSpinlock, 0);
}

void CriticaSectionlLock()
{
}

void CriticalSectionUnLock()
{
}

unsigned SpinLockLoopYeildThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock_LoopYield_Lock();
        if (g_num == LOOPCNT)
        {
            SpinLock_LoopYield_UnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinLock_LoopYield_UnLock();
    }
}

unsigned SpinLockYeildThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock_Yield_Lock();
        if (g_num == LOOPCNT)
        {
            SpinLock_Yield_UnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinLock_Yield_UnLock();
    }
}

unsigned SpinLockThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock();
        if (g_num == LOOPCNT)
        {
            SpinUnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinUnLock();
    }
}
