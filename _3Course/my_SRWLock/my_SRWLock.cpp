
#include <Windows.h>
#include <iostream>
#include <list>
#include <thread>

#pragma comment(lib, "Synchronization.lib")

#define THREAD_CNT 3
#define LOOP_CNT 100000000
using ull = volatile unsigned long long;

ull g_num;
HANDLE hStartEvent;

struct stMySRW
{
    PVOID addr;
};
struct stNode
{
    stNode(stNode *head, stNode *preNode, stNode *nextNode)
        : _head(head), _preNode(preNode), _nextNode(nextNode)
    {
    }
    ~stNode()
    {
    }
    stNode *_head;
    stNode *_preNode;
    stNode *_nextNode;
};
stMySRW srw_lock;
stNode *g_OldTail = nullptr;

ull llVal = 1;
ull bVisited = 1;

void AquireExclusive(stMySRW *srw)
{
    stNode *newNode;
    stNode *oldNode;
    int On = 1;

retry:
    // 소유권 획득
    if (_InterlockedCompareExchange((ull *)&srw->addr, (ull)1, (ull)0) == 0)
    {
        InterlockedExchange(&bVisited, 1);
        return;
    }
    else if (_InterlockedCompareExchange((ull *)&srw->addr, (ull)1, (ull)0) == 1)
    {
        while (InterlockedExchange(&llVal, 0) == 0)
        {
            YieldProcessor();
        }
        newNode = new stNode(nullptr, nullptr, nullptr);
        newNode->_head = newNode;

        srw_lock.addr = newNode;
        InterlockedExchange(&llVal, 1);

        // 누군가 진입한 상태면 Block
        WaitOnAddress(&bVisited, &On, sizeof(ull), INFINITE);
        goto retry;
    }
    // 특정 포인터 주소일 경우.
    else if (_InterlockedCompareExchange((ull *)&srw->addr, (ull)1, (ull)0) >= 65535)
    {
        while (InterlockedExchange(&llVal, 0) == 0)
        {
            YieldProcessor();
        }
        oldNode = reinterpret_cast<stNode *>(srw_lock.addr);
        newNode = new stNode(oldNode->_head, oldNode, nullptr);

        oldNode->_nextNode = newNode;

        srw_lock.addr = newNode;
        InterlockedExchange(&llVal, 1);
        // 누군가 진입한 상태면 Block
        WaitOnAddress(&bVisited, &On, sizeof(ull), INFINITE);
        goto retry;
    }
}
void ReleaseExclusive(stMySRW *srw)
{
    stNode *oldNode;
    ull retData;
    // 나 혼자만 들어가있었다면.
    if (_InterlockedCompareExchange((ull *)&srw->addr, (ull)0, (ull)1) == 1)
    {
        InterlockedExchange(&bVisited, 0);

        WakeByAddressSingle((PVOID)&bVisited);
        return;
    }
    else if (_InterlockedCompareExchange((ull *)&srw->addr, (ull)0, (ull)1) >= 65535)
    {
        InterlockedExchange(&bVisited, 0);

        while (InterlockedExchange(&llVal, 0) == 0)
        {
            YieldProcessor();
        }
        g_OldTail = reinterpret_cast<stNode *>(srw_lock.addr);

        InterlockedExchange(&llVal, 1);

        WakeByAddressSingle((PVOID)&bVisited);
    }
}

unsigned ExclusiveThread(void *arg)
{
    WaitForSingleObject(hStartEvent, INFINITE);

    for (int i = 0; i < LOOP_CNT; i++)
    {
        AquireExclusive(&srw_lock);
        g_num++;
        ReleaseExclusive(&srw_lock);
    }
    return 0;
}

void main()
{
    HANDLE hThread[THREAD_CNT];

    hStartEvent = CreateEvent(nullptr, true, 0, nullptr);
    for (int i = 0; i < THREAD_CNT; i++)
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, ExclusiveThread, nullptr, 0, nullptr);
    Sleep(1000);
    SetEvent(hStartEvent);

    WaitForMultipleObjects(THREAD_CNT, hThread, true, INFINITE);
    if (g_num != LOOP_CNT * THREAD_CNT)
    {
        __debugbreak();
    }
}
