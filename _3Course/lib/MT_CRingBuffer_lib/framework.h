#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

using ringBufferSize = _int64;

class CRingBuffer
{
  public:
    CRingBuffer();
    CRingBuffer(ringBufferSize iBufferSize);
    ~CRingBuffer();

    ringBufferSize GetUseSize(void);
    ringBufferSize GetFreeSize(void);
    bool ReSize(); // RecvBuffer ��  ���������� ��. SendBuffer�� ��� ƨ��°� �ǵ��� ����.

    bool Enqueue(const char *chpData, ringBufferSize iSize);
    bool Dequeue(char *chpDest, ringBufferSize iSize);

    ringBufferSize Peek(char *chpDest, ringBufferSize iSize);
    void ClearBuffer(void);

    ringBufferSize DirectEnqueueSize(void);
    ringBufferSize DirectDequeueSize(void);

    void MoveRear(ringBufferSize iSize);
    void MoveFront(ringBufferSize iSize);

  public:
    char *_begin;
    char *_end;

    char *_frontPtr;
    char *_rearPtr;
};
