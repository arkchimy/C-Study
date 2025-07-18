#include "CPeterson.h"
#include <iostream>

void CPeterson::PetersonLock(DWORD threadID, bool turn, bool other)
{
    long local_flag;
    long local_turn;
    int rand_type = rand() % 3;
    m_flag[turn] = true; // store m_flag  1
    m_turn = threadID;   // store m_turn  2
    LARGE_INTEGER currentTime, endTime, feq;
    currentTime.QuadPart = 0;

    MemoryBarrier();
    __faststorefence();
    _mm_mfence();

    QueryPerformanceCounter(&currentTime);
    switch (rand_type)
    {
    case 0:
        for (int i = 0; i < 1024 * 1024 * 1024; i++)
            MemoryBarrier();
        break;
    case 1:
        for (int i = 0; i < 1024 * 1024 * 1024; i++)
            _mm_mfence();
        break;
    case 2:
        for (int i = 0; i < 1024 * 1024 * 1024; i++)
            __faststorefence();
        break;
    }

    QueryPerformanceCounter(&endTime);
    total.QuadPart = endTime.QuadPart - currentTime.QuadPart;
    QueryPerformanceFrequency(&feq);
    switch (rand_type)
    {
    case 0:
        printf("  MemoryBarrier() %20.3lf us \n", total.QuadPart * (1e6 / feq.QuadPart) / (1024 * 1024 * 1024));
        break;

    case 1:
        printf("_mm_mfence :  %20.3lf us \n", total.QuadPart * (1e6 / feq.QuadPart) / (1024 * 1024 * 1024));
        break;
    case 2:
        printf("__faststorefence :  %20.3lf us \n", total.QuadPart * (1e6 / feq.QuadPart) / (1024 * 1024 * 1024));
        break;
    }

    while (1)
    {
        local_flag = m_flag[other]; // store local_flag , load m_flag[other] 아무와도 관련없음
        if (local_flag == false)    // load local_flag
        {
            InfoUpdate(EMode::Flag, local_flag);
            break;
        }
        local_turn = m_turn;        // store local_turn , load m_turn 아무와도 관련없음
        if (local_turn != threadID) // load local_turn  3
        {
            InfoUpdate(EMode::Turn, local_turn);
            break;
        }
    }
    if (InterlockedExchange(&bCSEnterFlag, 1) == 1) // 동시 진입 체크
        __debugbreak();
}

void CPeterson::PetersonUnLock(bool turn)
{
    long enterFlag = InterlockedExchange(&bCSEnterFlag, 0); // 동시 해제 체크 , 필요한가?
    InfoUpdate(EMode::UnLock, 0);

    if (enterFlag == 0)
        __debugbreak();

    m_flag[turn] = false;
    //_mm_mfence();
}

void CPeterson::InfoUpdate(EMode mode, int val)
{
    stDebugInfo info;
    ull seqNum;

    info.threadID = GetCurrentThreadId();
    info.mode = mode;
    info.val = val;

    seqNum = _InterlockedIncrement(&info_seqNum);
    info.seqNum = seqNum;
    infos[seqNum % INFOSIZE] = info;
}
