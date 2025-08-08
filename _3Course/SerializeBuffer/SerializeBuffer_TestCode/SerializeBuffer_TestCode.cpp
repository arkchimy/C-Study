
#include "../../../error_log.h"
#include "../../lib/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include <conio.h>
#include <iostream>
#include <queue>
#include <thread>
#include <vector>

std::queue<CMessage *> q;
SRWLOCK srw_q;
HANDLE hExitEvent;
// Consumer
unsigned Consumer(void *arg)
{
    CMessage *msg;
    {
    }
    DWORD cnt;
    DWORD val, wait_retval;

    srand(1);

    while (1)
    {
        std::vector<DWORD> vec;
        wait_retval = WaitForSingleObject(hExitEvent, 1000);
        
            

        AcquireSRWLockExclusive(&srw_q);
       
        if (q.empty())
        {
            if (wait_retval == WAIT_OBJECT_0)
            {
                ReleaseSRWLockExclusive(&srw_q);
                break;
            }
            ReleaseSRWLockExclusive(&srw_q);
            continue;
        }
        msg = q.front();
        q.pop();
        ReleaseSRWLockExclusive(&srw_q);

        cnt = rand() % 300;
        for (DWORD i = 0; i < cnt; i++)
        {
            vec.push_back(rand() % 100);
        }

        for (auto ele : vec)
        {
            *msg >> val;
            if (ele != val)
            {
                ERROR_FILE_LOG(L"dSerializeError.txt", L"val is not equle ele");
                printf("니 바보임?\n");
            }
        }
        printf("queue Size : %d \n" , q.size());
        delete msg;
    }
    return 0;
}
unsigned Producer(void *arg)
{
    DWORD cnt;
    DWORD wait_retval;
    srand(1);
    while (1)
    {
        wait_retval = WaitForSingleObject(hExitEvent, 100);
        if (wait_retval == WAIT_OBJECT_0)
            break;
        CMessage *msg = new CMessage();
        std::vector<DWORD> vec;
        cnt = rand() % 300;
        for (DWORD i = 0; i < cnt; i++)
        {
            vec.push_back(rand() % 100);
        }
        for (auto ele : vec)
        {
            *msg << ele;
        }

        AcquireSRWLockExclusive(&srw_q);
        q.push(msg);
        ReleaseSRWLockExclusive(&srw_q);
    }
    return 0;
}
int main()
{
    HANDLE hProducer, hConsumer;

    hExitEvent = CreateEvent(nullptr, 1, 0, nullptr);
    if (hExitEvent == nullptr)
        return -1;
    InitializeSRWLock(&srw_q);

    system("pause");

    hProducer = (HANDLE)_beginthreadex(nullptr, 0, Producer, nullptr, 0, nullptr);
    hConsumer = (HANDLE)_beginthreadex(nullptr, 0, Consumer, nullptr, 0, nullptr);

    HANDLE hThreads[] = {hProducer,
                         hConsumer};

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == VK_ESCAPE)
            {
                SetEvent(hExitEvent);
                break;
            }
        }
        YieldProcessor();
    }

    WaitForMultipleObjects(sizeof(hThreads) / sizeof(void*), hThreads, true, INFINITE);
}
