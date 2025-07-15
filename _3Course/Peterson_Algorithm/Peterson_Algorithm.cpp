#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>
#include "CPeterson.h"

HANDLE hStartEvent;


void CreatePetersonThread(); // 스레드 2개 생성하고 동시 시작 후 스레드가 리턴되면 반환
unsigned PetersonThread1(void *arg);
unsigned PetersonThread2(void *arg);


long g_num; // 공유 자원

int main()
{

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == 'a' || ch == 'A')
            {
                for (int i =0; i < 10 ; i++)
                    CreatePetersonThread();
            }
            if (ch == '0')
                return 0;
        }
    }
}

void CreatePetersonThread()
{
    HANDLE hThread[2];
    g_num = 0;
    printf("Start \n");
    hStartEvent = CreateEvent(nullptr, 1, false, nullptr);
    if (hStartEvent == 0)
        return;

    CPeterson peterson;

    hThread[0] = (HANDLE)_beginthreadex(nullptr, 0, PetersonThread1, (void *)&peterson, 0, nullptr);
    hThread[1] = (HANDLE)_beginthreadex(nullptr, 0, PetersonThread2, (void *)&peterson, 0, nullptr);
    if (hThread[0] == 0)
        __debugbreak;
    if (hThread[1] == 0)
        __debugbreak;

    Sleep(500);

    SetEvent(hStartEvent);

    WaitForMultipleObjects(2, hThread, true, INFINITE);
    printf("g_Num  :   %d \n", g_num);

    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);

   

}
unsigned PetersonThread1(void *arg)
{
    DWORD threadID = GetCurrentThreadId();
    CPeterson *peterson = reinterpret_cast<CPeterson *>(arg);

    WaitForSingleObject(hStartEvent, INFINITE);

    for (int i = 0; i < LoopCnt; i++)
    {
        peterson->PetersonLock(threadID,0, 1);
        g_num++;
        peterson->PetersonUnLock(0);
    }
    return 0;
}
unsigned PetersonThread2(void *arg)
{
    DWORD threadID = GetCurrentThreadId();
    CPeterson *peterson = reinterpret_cast<CPeterson *>(arg);
    WaitForSingleObject(hStartEvent, INFINITE);

    for (int i = 0; i < LoopCnt; i++)
    {
        peterson->PetersonLock(threadID,1, 0);
        g_num++;
        peterson->PetersonUnLock(1);
    }
    return 0;
}
