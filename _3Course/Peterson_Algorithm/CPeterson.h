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
    CPeterson() 
    {
        total.QuadPart = 0;
    }
    void PetersonLock(DWORD threadID, bool turn, bool other);
    void PetersonUnLock(bool turn);
    void InfoUpdate(EMode mode, int val);
    

  private:
    long m_turn;
    long m_flag[2];

    // Debug��
    long bCSEnterFlag = 0; // ���� ����
    ull info_seqNum;
    stDebugInfo infos[INFOSIZE];

    LARGE_INTEGER total;

};
