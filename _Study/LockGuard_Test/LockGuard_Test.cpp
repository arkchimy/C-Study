// LockGuard_Test.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

#include <conio.h>
#include <mutex>
#include <thread>
#include <Windows.h>

#include <unordered_set>
#include <string>

#include "../../_Portfolio/DeadLockGuard_lib/DeadLockGuard_lib.h"


HANDLE hStartEvent = CreateEvent(nullptr, true, false, nullptr);
HANDLE* hWaitEvent ;

long long g_idx = -1;
int num = 0;
bool bOn =true;




thread_local stTlsLockInfo tls_LockInfo;
SharedMutex m;
void workThread()
{
    long long idx = InterlockedIncrement64(&g_idx);
    MyMutexManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo);


    while (bOn)
    {
     
        WaitForSingleObject(hStartEvent, INFINITE);
        for (int i = 0; i < 10000; i++)
        {
            std::lock_guard<SharedMutex> m_lock(m);

            num++;

        }
        ResetEvent(hStartEvent);
        SetEvent(hWaitEvent[idx]);
    }

}
void workThread2()
{
    long long idx = InterlockedIncrement64(&g_idx);
    MyMutexManager::GetInstance()->RegisterTlsInfoAndHandle(&tls_LockInfo);

    while (bOn)
    {
        for (int i = 0; i < 10000; i++)
        {
            std::lock_guard<SharedMutex> m_lock(m);
            {
                {
                    std::lock_guard<SharedMutex> m_lock(m);
                }
            }
        }

    }
}
int main()
{
    int ThreadCnt;
    std::cout << "Thread 수 입력";
    std::cin >> ThreadCnt;
    std::thread *th = new std::thread[ThreadCnt];

    hWaitEvent = new HANDLE[ThreadCnt];

    for (int i = 0; i < ThreadCnt; i++)
    {
        std::wstring thName = L"WorkerThread";
        HANDLE waitEvent = CreateEvent(nullptr, false, false, nullptr);
        hWaitEvent[i] = waitEvent;
        th[i] = std::move(std::thread(workThread));
        SetThreadDescription(th[i].native_handle(), (thName + std::to_wstring(i)).c_str());
    }
    std::thread deadlockThread = std::move(std::thread(workThread2));
    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == 'a' || ch == 'A')
            {
                SetEvent(hStartEvent);
   /*             WaitForMultipleObjects(ThreadCnt, hWaitEvent,true, INFINITE);
                printf("Num : %d\n", num);
                num = 0;*/
            }
            if (ch == 'l' || ch == 'L')
            {
                MyMutexManager::GetInstance()->LogTlsInfo();
            }
            if (ch == VK_ESCAPE)
            {
                InterlockedExchange8((char *)&bOn, false);
                SetEvent(hStartEvent);
                break;
            }
        }
    }
    for (int i = 0; i < ThreadCnt; i++)
    {
        th[i].join();
    }
}
// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁:
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
