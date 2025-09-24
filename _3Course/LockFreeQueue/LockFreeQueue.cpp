// LockFreeQueue.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "../lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
#include "CLockFreeQueue.h"

#include <iostream>

#include <list>
#include <queue>
#include <thread>

#define WORKERTHREADCNT 6

std::list<int> test_list;
std::list<int> original_list;

SRWLOCK srw_test_list;
SRWLOCK srw_original_list;

HANDLE hStartEvent;
HANDLE hListInitEvent;
unsigned TestThread(void *arg)
{
    CLockFreeQueue<int> *q;
    int input_data;

    q = reinterpret_cast<CLockFreeQueue<int> *>(arg);

    std::vector<int> vec;
    std::vector<int> outData_vec;
    srand(GetCurrentThreadId());

    WaitForSingleObject(hListInitEvent, INFINITE);
    while (1)
    {
        AcquireSRWLockExclusive(&srw_test_list);

        if (test_list.empty() == true)
            break;

        input_data = test_list.front();
        test_list.pop_front();
        vec.push_back(input_data);

        ReleaseSRWLockExclusive(&srw_test_list);
        Sleep(0);
    }

    ReleaseSRWLockExclusive(&srw_test_list);

    WaitForSingleObject(hStartEvent, INFINITE);

    for (auto input_data : vec)
    {
        q->Push(input_data);
        outData_vec.push_back(q->Pop());
    }
    

    AcquireSRWLockExclusive(&srw_original_list);
    for (auto out_data : outData_vec)
    {
        for (auto iter = original_list.begin(); iter != original_list.end(); iter++)
        {
            if (*iter == out_data)
            {
                original_list.erase(iter);
                break;
            }
        }
    }
    ReleaseSRWLockExclusive(&srw_original_list);
    
    printf("Thread id :  %d  Loop %zu \n", GetCurrentThreadId(), vec.size());
    return 0;
}
CDump dump;

int main()
{

    InitializeSRWLock(&srw_test_list);
    InitializeSRWLock(&srw_original_list);
    srand(3);
    int data_cnt;
    int input_data;

    ull LoopCnt = 0;

    HANDLE hWorkerThread[WORKERTHREADCNT];

    hStartEvent = CreateEvent(nullptr, 1, 0, nullptr);
    if (hStartEvent == nullptr)
        __debugbreak();
    hListInitEvent = CreateEvent(nullptr, 1, 0, nullptr);
    if (hListInitEvent == nullptr)
        __debugbreak();

    

    while (1)
    {
        data_cnt = rand() % 100000;
        for (int i = 0; i < data_cnt; i++)
        {
            input_data = rand() % 100;
            original_list.push_back(input_data);
            test_list.push_back(input_data);
        }

        CLockFreeQueue<int> q;
        for (int i = 0; i < WORKERTHREADCNT; i++)
            hWorkerThread[i] = (HANDLE)_beginthreadex(nullptr, 0, TestThread, &q, 0, nullptr);
        printf("===============================\n");
        Sleep(10);
        SetEvent(hListInitEvent);

        Sleep(10);
        SetEvent(hStartEvent);

        WaitForMultipleObjects(WORKERTHREADCNT, hWorkerThread, true, INFINITE);

        if (original_list.size() != 0)
            __debugbreak();

        if (test_list.size() != 0)
            __debugbreak();

        ResetEvent(hListInitEvent);
        ResetEvent(hStartEvent);

        printf("LoopCnt : %lld \n", ++LoopCnt);
        printf("===============================\n");
        for (int i = 0; i < WORKERTHREADCNT; i++)
            CloseHandle(hWorkerThread[i]);
    }
}
//
// int main()
//{
//    ull Cnt = 1;
//    int rand_num, loopCnt;
//
//    srand(3);
//    while (1)
//    {
//        CLockFreeQueue<int> q;
//        std::queue<int> stl_q;
//
//        loopCnt = rand() % 5;
//        for (int i = 0; i < loopCnt; i++)
//        {
//            rand_num = rand() % 521;
//
//            q.Push(rand_num);
//            stl_q.push(rand_num);
//        }
//        for (int i = 0; i < loopCnt; i++)
//        {
//            int test1, test2;
//
//            test1 = q.Pop();
//            test2 = stl_q.front();
//            stl_q.pop();
//            if (test1 != test2)
//                __debugbreak();
//        }
//        printf("Loop Cnt : %lld \n", Cnt++);
//    }
//}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁:
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
