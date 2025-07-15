#pragma once
#include <Windows.h>
//&infos[info_seqNum % 100]
#define LoopCnt 50000000
#define INFOSIZE 100

using ull = unsigned long long;

enum class EMode
{
    // � ������ �Ͽ�����
    UnLock = 0xDDDDDDDD,
    Turn = 0xEEEEEEEE,
    Flag = 0xFFFFFFFF,

    Max,
};
struct stDebugInfo
{
    DWORD seqNum = 0; // �������ѹ�
    DWORD threadID; // � �����尡

    EMode mode; // � ������
    int val;    // ���� �������� ���Ҵ���.
};

class CPeterson
{
  public:
    void PetersonLock(DWORD threadID,bool turn, bool other)
    {
        long local_flag;
        long local_turn;

        m_flag[turn] = true; // store m_flag  1
        m_turn = threadID;   // store m_turn  2

        //_mm_mfence();

        while (1)
        {
            local_flag = m_flag[other];// store local_flag , load m_flag[other] �ƹ��͵� ���þ���
            if (local_flag == false)   // load local_flag
            {
                InfoUpdate(EMode::Flag, local_flag);
                break;
            }
            local_turn = m_turn;        // store local_turn , load m_turn �ƹ��͵� ���þ���
            if (local_turn != threadID) // load local_turn  3
            {
                InfoUpdate(EMode::Turn, local_turn);
                break;
            }
        }
        if (InterlockedExchange(&bCSEnterFlag, 1) == 1) // ���� ���� üũ
            __debugbreak();
    }
    void PetersonUnLock(bool turn)
    {
        long enterFlag = InterlockedExchange(&bCSEnterFlag, 0); // ���� ���� üũ , �ʿ��Ѱ�?
        InfoUpdate(EMode::UnLock, 0);

        if (enterFlag == 0)
            __debugbreak();

        m_flag[turn] = false;
        //_mm_mfence();
    }
    void InfoUpdate(EMode mode, int val)
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
    long m_turn;
    long m_flag[2];

    long bCSEnterFlag = 0; // ���� ����
    ull info_seqNum;
    stDebugInfo infos[INFOSIZE];
};
