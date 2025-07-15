#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <thread>

#define LoopCnt 5000000
#define INFOSIZE 100
enum class EMode
{
    UnLock = 0xDDDDDDDD,
    Turn = 0xEEEEEEEE,
    Flag = 0xFFFFFFFF,

    Max,
};
struct stDebugInfo
{
    DWORD seqNum = 0;
    DWORD threadID; // 어떤 쓰레드가

    EMode mode; // 어떤 변수를
    int val;    // 값을 무엇으로 보았는지.
};

HANDLE hStartEvent;
stDebugInfo infos[INFOSIZE];

long info_seqNum = 0;  //
long bCSEnterFlag = 1; // 진입 여부

int g_flag[2];
int g_turn;
int g_num;

void CreatePetersonThread(); // 스레드 2개 생성하고 동시 시작 후 스레드가 리턴되면 반환

void Lock0();
void UnLock0();

void Lock1();
void UnLock1();

unsigned Thread0(void *arg);
unsigned Thread1(void *arg);

int main()
{

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == 'a' || ch == 'A')
            {
                CreatePetersonThread();
            }
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

    hThread[0] = (HANDLE)_beginthreadex(nullptr, 0, Thread0, nullptr, 0, nullptr);
    hThread[1] = (HANDLE)_beginthreadex(nullptr, 0, Thread1, nullptr, 0, nullptr);
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

void Lock0()
{
    stDebugInfo info;
    long seqNum;
    long enterCnt;
    int flag, turn;
    DWORD threadId = GetCurrentThreadId();

    g_flag[0] = true;
    g_turn = 0;

    while (1)
    {
        flag = g_flag[1];
        if (flag == false)
        {
            info.threadID = threadId;
            info.mode = EMode::Flag;
            info.val = flag;
            break;
        }

        turn = g_turn;
        if (turn != 0)
        {
            info.threadID = threadId;
            info.mode = EMode::Turn;
            info.val = turn;
            break;
        }
    }

    seqNum = InterlockedIncrement(&info_seqNum);
    info.seqNum = seqNum;
    infos[seqNum % INFOSIZE] = info;

    if (InterlockedExchange(&bCSEnterFlag, 0) == 0)
    {
        __debugbreak();
    }

}

void UnLock0()
{
    stDebugInfo info;
    long seqNum;

    info.threadID = GetCurrentThreadId();
    info.mode = EMode::UnLock;
    info.val = 0;

    seqNum = InterlockedIncrement(&info_seqNum);
    info.seqNum = seqNum;
    infos[seqNum % INFOSIZE] = info;

    if (InterlockedExchange(&bCSEnterFlag, 1) == 1)
        __debugbreak();

    g_flag[0] = false;
}
void Lock1()
{
    stDebugInfo info;
    long seqNum;
    DWORD threadId = GetCurrentThreadId();

    g_flag[1] = true;
    g_turn = 1;
    while (1)
    {
        if (g_flag[0] == false)
        {
            info.threadID = threadId;
            info.mode = EMode::Flag;
            info.val = false;
            break;
        }
        if (g_turn != 1)
        {
            info.threadID = GetCurrentThreadId();
            info.mode = EMode::Turn;
            info.val = 0;
            break;
        }
    }
    seqNum = InterlockedIncrement(&info_seqNum);
    info.seqNum = seqNum;
    infos[seqNum % INFOSIZE] = info;
    if (InterlockedExchange(&bCSEnterFlag, 0) == 0)
    {
        __debugbreak();
    }

}
void UnLock1()
{
    stDebugInfo info;
    long seqNum;

    info.threadID = GetCurrentThreadId();
    info.mode = EMode::UnLock;
    info.val = 0;

    seqNum = InterlockedIncrement(&info_seqNum);
    info.seqNum = seqNum;
    infos[seqNum % INFOSIZE] = info;

    if (InterlockedExchange(&bCSEnterFlag, 1) == 1)
        __debugbreak();

    g_flag[1] = false;
}
unsigned Thread0(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    for (int i = 0; i < LoopCnt; i++)
    {
        Lock0();

        g_num++;

        UnLock0();
    }
    return 0;
}

unsigned Thread1(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    for (int i = 0; i < LoopCnt; i++)
    {
        Lock1();

        g_num++;

        UnLock1();
    }
    return 0;
}
