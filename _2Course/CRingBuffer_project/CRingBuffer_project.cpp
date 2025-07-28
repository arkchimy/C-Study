// CRingBuffer_project.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "CRingBuffer.h"
#include <conio.h>
#include <iostream>
#include <string>
#include <thread>
#include <windows.h >
#include "../../_1Course/lib/Parser_lib/Parser_lib.h"

CRingBuffer ringBuffer(17);

HANDLE hEnQThread[2];
HANDLE hDeQThread[2];

HANDLE hStartEvent;
HANDLE hExitEvent;
bool bOn = true;

unsigned EnQThread(void *arg)
{
    srand(1);
    long num;
    bool bSuccess = true;
    DWORD retval;

    WaitForSingleObject(hStartEvent, INFINITE);
    while (1)
    {
        retval = WaitForMultipleObjects(2, hEnQThread, false, INFINITE);
        if (retval == WAIT_OBJECT_0 + 1)
            return 0;

        if (bSuccess)
            num = rand() % 100;
        bSuccess = ringBuffer.Enqueue(reinterpret_cast<char *>(&num), sizeof(long));
    }

    return 0;
}
unsigned DeQThread(void *arg)
{
    srand(1);
    long num;
    long cmpNum;
    unsigned long long cnt = 0;
    char LogBuffer[1000];

    DWORD retval;

    WaitForSingleObject(hStartEvent, INFINITE);
    while (1)
    {
        retval = WaitForMultipleObjects(2, hDeQThread, false, INFINITE);
        if (retval == WAIT_OBJECT_0 + 1)
            return 0;

        if (ringBuffer.Dequeue(reinterpret_cast<char *>(&cmpNum), sizeof(long)) == true)
        {
            cnt++;

            printf("DeQue cnt : %lld  \t   UseSize :  % lld \n", cnt, ringBuffer.GetUseSize());
            num = rand() % 100;
            if (num != cmpNum)
            {
                FILE *file;

                fopen_s(&file, "Error Detect.txt", "a+");
                if (file == nullptr)
                    return -3;
                std::string s = "Cnt : ";
                s.append(std::to_string(cnt));
                s.append("\n");

                fwrite(s.c_str(), 1, s.size(), file);
                fclose(file);
                __debugbreak();
                return -1;
            }
        }
    }
    return 0;
}

void TogleEnQThread()
{
    static int a = 0;
    if (a == 0)
    {
        SetEvent(hEnQThread[0]);
        a = 1;
    }
    else
    {
        ResetEvent(hEnQThread[0]);
        a = 0;
    }
}
void TogleDeQThread()
{
    static int a = 0;
    if (a == 0)
    {
        SetEvent(hDeQThread[0]);
        a = 1;
    }
    else
    {
        ResetEvent(hDeQThread[0]);
        a = 0;
    }
}

int main()
{

    {
        HANDLE hThread[2];

        hStartEvent = CreateEvent(nullptr, true, false, nullptr);
        hExitEvent = CreateEvent(nullptr, true, false, nullptr);

        hEnQThread[0] = CreateEvent(nullptr, true, false, nullptr);
        hDeQThread[0] = CreateEvent(nullptr, true, false, nullptr);

        hEnQThread[1] = hExitEvent;
        hDeQThread[1] = hExitEvent;

        hThread[0] = (HANDLE)_beginthreadex(nullptr, 0, EnQThread, nullptr, 0, nullptr);
        hThread[1] = (HANDLE)_beginthreadex(nullptr, 0, DeQThread, nullptr, 0, nullptr);

        Sleep(1000);
        SetEvent(hStartEvent);
        while (1)
        {
            if (_kbhit())
            {
                char ch = _getch();
                switch (ch)
                {
                case 'e':
                case 'E':
                    TogleEnQThread();
                    break;
                case 'd':
                case 'D':
                    TogleDeQThread();
                    break;

                case VK_ESCAPE:
                    ResetEvent(hEnQThread[0]);
                    ResetEvent(hDeQThread[0]);
                    SetEvent(hExitEvent);
                    goto exitline;
                }
            }
        }
    exitline:
        WaitForMultipleObjects(2, hThread, true, INFINITE);
    }
}
