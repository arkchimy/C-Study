// WaitforsingleObjectEX_Test.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>

HANDLE hWaitEvent, hStart;
HANDLE hThread;
ULONG Apc_Cnt ;

VOID CALLBACK APCProc(ULONG_PTR dwParam)
{
    printf("APC called! : %ld \n", dwParam);
}
unsigned __stdcall WorkerThread(void *arg)
{
    static LONG num = 1;
    DWORD current = GetCurrentThreadId();
    DWORD retval;
    WaitForSingleObject(hStart, INFINITE);

    while (1)
    {
        retval = WaitForSingleObjectEx(hWaitEvent, INFINITE, true);
        if (retval == WAIT_OBJECT_0)
            printf("Logic called! : %d \n", num++);
        else if (retval == WAIT_IO_COMPLETION)
        {
            for (int i = 0; i < 1000; i++)
            {
                __assume(hThread != 0);

                if (0 == QueueUserAPC(APCProc, hThread, Apc_Cnt++))
                    __debugbreak();
            }
        }
    }

    return 0;
}

int main()
{
   

    hWaitEvent = CreateEvent(nullptr, true, 1, nullptr);
    hStart = CreateEvent(nullptr, true, 0, nullptr);

    hThread = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    if (hThread == nullptr)
        return -1;


    if (hWaitEvent == nullptr)
        return -1;
    Sleep(5000);

    while (1)
    {
        for (int i = 0; i < 1000; i++)
        {
            __assume(hThread != 0);

            if (0 == QueueUserAPC(APCProc, hThread, Apc_Cnt++))
                __debugbreak();
        }
        //Sleep(100);
    }
   
    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == VK_EXECUTE)
                break;
            else if (ch == 's' || ch == 'S')
                SetEvent(hStart);
     
        }
    }
}
