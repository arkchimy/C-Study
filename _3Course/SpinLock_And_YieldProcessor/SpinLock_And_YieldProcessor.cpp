

#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>
#include <vector>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"

#pragma comment(lib, "winmm.lib")

void SpinLock();
void SpinUnLock();

void SpinLock_Sleep();
void SpinUnLock_Sleep();

void SpinLock_Yield_Lock();
void SpinLock_Yield_UnLock();

void SpinLock_LoopYield_Lock();
void SpinLock_LoopYield_UnLock();

int THREADCNT;
int TARGETCNT;
int LOOPCNT = 1;

alignas(64) long lSpinlock;

HANDLE hStartEvent;
HANDLE hEndEvent;
LARGE_INTEGER startTime, endTime, total, Freq;

alignas(64) long g_num;

unsigned SpinLockThread(void *arg);
unsigned SpinLockYeildThread(void *arg);
unsigned SpinLockIFYeildThread(void *arg);
unsigned SpinLockLoopYieldThread(void *arg);
unsigned SpinLockSleepThread(void *arg);

void MyCreateThread(unsigned (*type)(void *))
{

    std::vector<HANDLE> hThread;
    hThread.resize(THREADCNT);

    QueryPerformanceFrequency(&Freq);

    for (int i = 0; i < THREADCNT; i++)
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, type, nullptr, 0, nullptr);

    Sleep(300);

    g_num = 0;
    QueryPerformanceCounter(&startTime);

    SetEvent(hStartEvent);
    WaitForSingleObject(hEndEvent, INFINITE);

    QueryPerformanceCounter(&endTime);

    WaitForMultipleObjects(THREADCNT, &hThread[0], true, INFINITE);
    ResetEvent(hStartEvent);
    ResetEvent(hEndEvent);
}

int main()
{
    timeBeginPeriod(1);

    {
        Parser parser;
        parser.LoadFile(L"Config.txt");
        parser.GetValue(L"THREADCNT", THREADCNT);
        parser.GetValue(L"LOOPCNT", LOOPCNT);
        parser.GetValue(L"TARGETCNT", TARGETCNT);
    }
    hStartEvent = CreateEvent(nullptr, 1, false, nullptr);
    hEndEvent = CreateEvent(nullptr, 1, false, nullptr);

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            LARGE_INTEGER total;
            total.QuadPart = 0;
            printf("Start Logic\n");
            switch (ch)
            {
            case '1':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockThread);

                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" g_Num : %d \t SpinLockThread time %20.3lf us \n", g_num, total.QuadPart * (1e6 / Freq.QuadPart) / LOOPCNT);
                break;
            case '2':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockYeildThread);

                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" g_Num : %d \t  SpinLockYeildThread time %20.3lf us \n", g_num, total.QuadPart * (1e6 / Freq.QuadPart) / LOOPCNT);
                break;
            case '3':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockIFYeildThread);

                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" g_Num : %d \t  SpinLockIFYeildThread time %20.3lf us \n", g_num, total.QuadPart * (1e6 / Freq.QuadPart) / LOOPCNT);
                break;
            case '4':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockLoopYieldThread);

                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" g_Num : %d \t  SpinLockLoopYieldThread time %20.3lf us \n", g_num, total.QuadPart * (1e6 / Freq.QuadPart) / LOOPCNT);
                break;
            case '5':
                for (int i = 0; i < LOOPCNT; i++)
                {
                    MyCreateThread(SpinLockSleepThread);

                    total.QuadPart += endTime.QuadPart - startTime.QuadPart;
                }
                printf(" g_Num : %d \t  SpinLockSleepThread time %20.3lf us \n", g_num, total.QuadPart * (1e6 / Freq.QuadPart) / LOOPCNT);
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

void SpinLock_Sleep()
{
    while (_InterlockedExchange(&lSpinlock, 1) == 1)
    {
        Sleep(0);
    }
}

void SpinUnLock_Sleep()
{
    _InterlockedExchange(&lSpinlock, 0);
}

void SpinLock_IFYield_Lock()
{
    while (1)
    {
        if (lSpinlock == 0)
        {
            if (_InterlockedExchange(&lSpinlock, 1) == 1)
                YieldProcessor();
            else
                break;
        }
        YieldProcessor();
    }
}

void SpinLock_IFYield_UnLock()
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

unsigned SpinLockLoopYieldThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock_LoopYield_Lock();
        if (g_num == TARGETCNT)
        {
            SpinLock_LoopYield_UnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinLock_LoopYield_UnLock();
    }
}

unsigned SpinLockSleepThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock_Sleep();
        if (g_num == TARGETCNT)
        {
            SpinUnLock_Sleep();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinUnLock_Sleep();
    }
}

unsigned SpinLockYeildThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock_Yield_Lock();
        if (g_num == TARGETCNT)
        {
            SpinLock_Yield_UnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinLock_Yield_UnLock();
    }
}

unsigned SpinLockIFYeildThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock_IFYield_Lock();
        if (g_num == TARGETCNT)
        {
            SpinLock_IFYield_UnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinLock_IFYield_UnLock();
    }
}

unsigned SpinLockThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    while (1)
    {
        SpinLock();
        if (g_num == TARGETCNT)
        {
            SpinUnLock();
            SetEvent(hEndEvent);
            return 0;
        }
        g_num++;
        SpinUnLock();
    }
}
