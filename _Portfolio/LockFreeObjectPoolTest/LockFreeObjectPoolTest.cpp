#include <iostream>
#include "../../_3Course/lib/CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"
#include <thread>

/*
* ObjectPool을 여러 쓰레드에서 Alloc을 받고 Release를 한다.
* 예상 문제 상황 : 
  1. 현재 할당된 Node가 이미 누군가 소유하고 있는경우.
  2. 이미 반환된 Node가 또 반환 되는 경우
*/


HANDLE hWaitEvent;

unsigned WorkerThread(void *arg) 
{
    CObjectPool<int> pool;
    std::vector<int*> nums;

    WaitForSingleObject(hWaitEvent, INFINITE);

    for (int i = 0; i < 10000; i++)
    {
        nums.push_back(reinterpret_cast<int*>(pool.Alloc()));
    }
    for (auto element : nums)
    {
        pool.Release(element);
    }

    return 0;
}
#define WORKER_CNT 10
int main()
{
    HANDLE* hThread;
    hThread = new HANDLE[WORKER_CNT];

    for (int i = 0; i < WORKER_CNT; i++)
        hThread[i] =(HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    hWaitEvent = CreateEvent(nullptr, 1, false, nullptr);
    Sleep(1000);
    SetEvent(hWaitEvent);

    WaitForMultipleObjects(WORKER_CNT, hThread,true,INFINITE);


}
