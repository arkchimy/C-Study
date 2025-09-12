#include "../../_3Course/lib/CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"

#include <iostream>
#include <thread>

/*
* ObjectPool을 여러 쓰레드에서 Alloc을 받고 Release를 한다.
* 예상 문제 상황 :
  1. 현재 할당된 Node가 이미 누군가 소유하고 있는경우.
  2. 이미 반환된 Node가 또 반환 되는 경우
*/

CObjectPool<int> pool;
HANDLE hWaitEvent;
bool bOn = true;

unsigned WorkerThread(void *arg)
{

    std::vector<int *> nums;

    WaitForSingleObject(hWaitEvent, INFINITE);

    while (bOn)
    {
        for (int i = 0; i < 100; i++)
        {
            int *num = reinterpret_cast<int *>(pool.Alloc());
            nums.push_back(num);
        }
        for (auto element : nums)
        {
            pool.Release(element);
        }
        nums.clear();
    }

    return 0;
}
#define WORKER_CNT 10

CDump dump;
int main()
{
    HANDLE *hThread;

    hThread = new HANDLE[WORKER_CNT];

    for (int i = 0; i < WORKER_CNT; i++)
        hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    hWaitEvent = CreateEvent(nullptr, 1, false, nullptr);
    Sleep(1000);
    SetEvent(hWaitEvent);

    while (1)
    {
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            bOn = false;
            break;
        }
    }
    WaitForMultipleObjects(WORKER_CNT, hThread, true, INFINITE);
}
