#pragma once
using ringBufferSize = _int64;

class CRingBuffer
{
  public:
    enum enBufferMode
    {
        Recv,
        Send,
        Max,
    };

  public:
    CRingBuffer() = delete;

    CRingBuffer(enBufferMode mode);
    CRingBuffer(ringBufferSize iBufferSize, enBufferMode mode);
    ~CRingBuffer();

    ringBufferSize GetUseSize(void);
    ringBufferSize GetFreeSize(void);
    bool ReSize(); // RecvBuffer 만  고유적으로 됨. SendBuffer의 경우 튕기는걸 의도할 것임.

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

    enBufferMode _mode;
};
