#pragma once
#include <memory>
#include <Windows.h>   
#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

using ringBufferSize = int;


class CRingBuffer
{
  public:
    CRingBuffer();
    CRingBuffer(ringBufferSize iBufferSize);
    ~CRingBuffer();

    ringBufferSize GetUseSize();
    ringBufferSize GetUseSize(const char *f, const char *r);
    ringBufferSize GetFreeSize();
    ringBufferSize GetFreeSize(const char *f, const char *r);

    ringBufferSize Enqueue(const void *chpData, ringBufferSize iSize);
    ringBufferSize Dequeue(void *chpDest, ringBufferSize iSize);

    ringBufferSize Peek(void *chpDest, ringBufferSize iSize);
    void ClearBuffer();

    ringBufferSize DirectEnqueueSize();
    ringBufferSize DirectEnqueueSize(const char* f, const char* r);

    ringBufferSize DirectDequeueSize();
    ringBufferSize DirectDequeueSize(const char *f, const char *r);

    void MoveRear(ringBufferSize iSize);
    void MoveFront(ringBufferSize iSize);

  public:
    char *_begin;
    char *_end;

    char *_frontPtr;
    char *_rearPtr;
};
