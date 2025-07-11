// _MultiThread_TimebeginPeriod_.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "winmm.lib")

#define THREADCNT 4
long long g_num;
bool bOn = true;

HANDLE hStartEvent;

struct stTimePeriod
{
    stTimePeriod()
    {
        timeBeginPeriod(1);
    }
    ~stTimePeriod()
    {
        timeEndPeriod(1);
    }
};
unsigned Add(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);
    while (bOn)
    {
        InterlockedIncrement64(&g_num);
    }
    return 0;
}
unsigned BlockThread(void *arg)
{

    WaitForSingleObject(hStartEvent, INFINITE);

    while (bOn)
    {
        Sleep(0);
    }
    return 0;
}

unsigned MonitorThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    DWORD currentTime = timeGetTime();
    DWORD nextTime = currentTime + 1000;

    long long toTal = 0;

    while (bOn)
    {

        currentTime = timeGetTime();
        if (nextTime <= currentTime)
        {
            if (toTal == 0)
            {
                toTal = g_num;
            }
            else
            {
                toTal += g_num;
                toTal /= 2;
            }
            printf("g_num %lld Aver : %lld \n", g_num , toTal);
            InterlockedExchange64(&g_num, 0);

            nextTime += 1000;
        }
    }
    return 0;
}

stTimePeriod stPeriod;

int main()
{
    for (int i = 0; i < THREADCNT; i++)
        _beginthreadex(nullptr, 0, BlockThread, nullptr, 0, nullptr);
    _beginthreadex(nullptr, 0, Add, nullptr, 0, nullptr);
    _beginthreadex(nullptr, 0, MonitorThread, nullptr, 0, nullptr);

    hStartEvent = CreateEvent(nullptr, true, 0, nullptr);
    if (hStartEvent == nullptr)
    {
        __debugbreak();
    }
    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();

            if (ch == 'a' || ch == 'A')
            {
                SetEvent(hStartEvent);
            }
            if (ch == 's' || ch == 'S')
            {
                bOn = FALSE;
                return 0;
            }
        }
    }


}
