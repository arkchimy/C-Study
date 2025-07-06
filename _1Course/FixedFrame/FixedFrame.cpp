// FixedFrame.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>

#include <map>

#pragma comment(lib, "winmm.lib")

#define FRAMETIME 20

volatile LONG g_Speed = 0;

unsigned inputThread(void *arg)
{
    while (1)
    {

        if (_kbhit())
        {
            char ch = _getch();
            if (ch == 'a')
            {
                InterlockedIncrement(&g_Speed);
            }
            else if (ch == 'd')
            {
                InterlockedDecrement(&g_Speed);
            }
        }
    }
}

void Logic()
{
    Sleep(10);
}
void Render()
{
    volatile long currentSpeed = g_Speed;
    Sleep(currentSpeed);
}
int main()
{
    timeBeginPeriod(1);
    _beginthreadex(nullptr, 0, &inputThread, nullptr, 0, nullptr);

    DWORD startTime = timeGetTime();
    DWORD currentTime, nextTime;

    DWORD sleepCnt = 0;

    DWORD iMonitorTime = timeGetTime();
    DWORD Frame = 0;
    DWORD SleepCnt = 0;
    DWORD skipCnt = 0;

    currentTime = timeGetTime();
    nextTime = currentTime;

    bool bSkip = false;

    while (1)
    {
        Logic();
        if (bSkip == false)
            Render();
        else
            skipCnt++;

        Frame++;
        nextTime += FRAMETIME;

        if (timeGetTime() >= iMonitorTime + 1000)
        {
            static std::map<DWORD,DWORD> m;

            printf("FrameCnt : %d \t SleepCnt   :  %d  skipCnt : %d  gSpeed    : %d \n", Frame, SleepCnt, skipCnt, g_Speed);
            m[Frame]++;
            for (auto data : m)
            {
                printf(" %d  Frame : |  %d  |\n", data.first, data.second);
            }
            printf("\n");
            iMonitorTime += 1000;
            Frame = 0;
            SleepCnt = 0;
        }

        currentTime = timeGetTime();
        if (nextTime > currentTime)
        {
            SleepCnt++;
            Sleep(nextTime - currentTime);
            bSkip = false;
        }
        else
        {
            bSkip = true;
        }
    }
}
