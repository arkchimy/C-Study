#pragma once
#include <Windows.h>
//&infos[info_seqNum % 100]
#define LoopCnt 50000000
#define INFOSIZE 100

using ull = unsigned long long;

enum class EMode
{
    // 어떤 동작을 하였는지
    UnLock = 0xDDDDDDDD,
    Turn = 0xEEEEEEEE,
    Flag = 0xFFFFFFFF,

    Max,
};
struct stDebugInfo
{
    DWORD seqNum = 0; // 시퀀스넘버
    DWORD threadID; // 어떤 쓰레드가

    EMode mode; // 어떤 변수를
    int val;    // 값을 무엇으로 보았는지.
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

    // Debug용
    long bCSEnterFlag = 0; // 진입 여부
    ull info_seqNum;
    stDebugInfo infos[INFOSIZE];

    LARGE_INTEGER total;

};
