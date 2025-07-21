
#include <Windows.h>
#include <conio.h>
#include <iostream>
#include <list>
#include <thread>

std::list<size_t> list;

SRWLOCK srw_lock;

/*
 * 실험 내용
 * Shared가 계속해서 호출 된다면, AcquireSRWLockExclusive 는 기아상태에 빠지게되는가?
 */
HANDLE hExclusive;
HANDLE hShared;
unsigned int InsertThread(void *arg)
{
    size_t num = 1;

    while (1)
    {
        WaitForSingleObject(hExclusive, INFINITE);

        AcquireSRWLockExclusive(&srw_lock);
        printf("Exclusive %zu insert\n", num++);
        

        ReleaseSRWLockExclusive(&srw_lock);
    }

    return 0;
}
unsigned int SearchThread(void *arg)
{
    WaitForSingleObject(hShared, INFINITE);

    while (1)
    {
        AcquireSRWLockShared(&srw_lock);

        ReleaseSRWLockShared(&srw_lock);
    }

    return 0;
}
int main()
{
    for (size_t i = 0; i < 10; i++)
    {
        list.push_back(i);
    }
    InitializeSRWLock(&srw_lock);
    for (int i = 0; i < 5; i++)
        _beginthreadex(nullptr, 0, InsertThread, nullptr, 0, nullptr);
    for (int i = 0; i < 5; i++)
    {
        _beginthreadex(nullptr, 0, SearchThread, nullptr, 0, nullptr);
    }
    hExclusive = CreateEvent(nullptr, 1, false, nullptr);
    hShared = CreateEvent(nullptr, 1, false, nullptr);
    Sleep(1000);

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == 'a' || ch == 'A')
                SetEvent(hExclusive);
            else if (ch == 'c' || ch == 'C')
                SetEvent(hShared);
        }
        YieldProcessor();
    }
}
