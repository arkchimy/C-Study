#include "CRingBuffer.h"
#include <memory>

CRingBuffer::CRingBuffer(enBufferMode mode)
{
    _mode = mode;
    _begin = _mode == enBufferMode::Recv ? (char *)_aligned_malloc(2048 * 2, 4096)
                                         : (char *)_aligned_malloc(2048, 4096);
    _end = _begin + 1001;
    ClearBuffer();
}

CRingBuffer::CRingBuffer(ringBufferSize iBufferSize, enBufferMode mode)
{
    /*  ringBufferSize realSize = iBufferSize % 4096;
      realSize = realSize == 0 ? iBufferSize : iBufferSize + realSize;*/

    _mode = mode;
    _begin = _mode == enBufferMode::Recv ? (char *)_aligned_malloc(iBufferSize * 2, 4096)
                                         : (char *)_aligned_malloc(iBufferSize, 4096);
    _end = _begin + iBufferSize;
    ClearBuffer();
}

CRingBuffer::~CRingBuffer()
{
    _aligned_free(_begin);
}

ringBufferSize CRingBuffer::GetUseSize(void)
{
    char *f = _frontPtr;
    char *r = _rearPtr;

    return f <= r ? r - f
                  : _end - f + r - _begin;
}

ringBufferSize CRingBuffer::GetFreeSize(void)
{
    // 가득 채우면 안되므로 -1
    char *f = _frontPtr;
    char *r = _rearPtr;

    return f <= r ? (_end - r) + (f - _begin) - 1
                  : f - r - 1;
}

bool CRingBuffer::ReSize()
{
    char *f = _frontPtr, *r = _rearPtr;

    if (_mode == Send)
        return false;

    // TODO : 이게 멀쓰에서 안전한가?
    /* if (f <= r)
     {
         _end += _end - _begin;

     }*/

    return false;
}

bool CRingBuffer::Enqueue(const char *chpSrc, ringBufferSize iSize)
{
    ringBufferSize directSize = DirectEnqueueSize();
    char *f = _frontPtr, *r = _rearPtr;
    
    if (GetFreeSize() <= iSize)
    {
        bool bChk = ReSize();
        if (bChk == false)
            return false;
    }

    if (iSize <= directSize)
    {
        memcpy(r, chpSrc, iSize);
    }
    else
    {
        memcpy(r, chpSrc, directSize);
        memcpy(_begin, chpSrc + directSize, iSize - directSize);
    }
    MoveRear(iSize);

    return true;
}

bool CRingBuffer::Dequeue(char *chpDest, ringBufferSize iSize)
{
    char *f, *r;
    ringBufferSize DirectDeqSize;
    ringBufferSize useSize = GetUseSize();
    if (useSize < iSize)
        return false;

    //TODO : 비순차적 문제는 없을까? Store라 괜찮음

    DirectDeqSize = DirectDequeueSize();

    f = _frontPtr;
    r = _rearPtr;

    if (iSize <= DirectDeqSize)
    {
        memcpy(chpDest, f, iSize);
    }
    else
    {
        memcpy(chpDest, f, DirectDeqSize);
        memcpy(chpDest + DirectDeqSize, _begin, iSize - DirectDeqSize);
    }
    MoveFront(iSize);

    return true;
}

ringBufferSize CRingBuffer::Peek(char *chpDest, ringBufferSize iSize)
{
    return ringBufferSize();
}

void CRingBuffer::ClearBuffer(void)
{
    _rearPtr = _begin;
    _frontPtr = _begin;
}

ringBufferSize CRingBuffer::DirectEnqueueSize(void)
{
    char *f = _frontPtr, *r = _rearPtr;

    if (f == _begin)
    {
        return _end - r - 1;
    }
    return f <= r ? _end - r
                  : f - r - 1;
}

ringBufferSize CRingBuffer::DirectDequeueSize(void)
{
    char *f = _frontPtr, *r = _rearPtr;

    return f <= r ? r - f : _end - f;
}

void CRingBuffer::MoveRear(ringBufferSize iSize)
{
    char* pChk = _rearPtr + iSize;
    char *distance = reinterpret_cast<char *>(pChk - _end);
   
    if (_end <= pChk)
    {
        char *distance = reinterpret_cast<char *>(pChk - _end);

        _rearPtr = _begin + long(distance);
    }
    else
    {
        _rearPtr = pChk;
    }
  
}

void CRingBuffer::MoveFront(ringBufferSize iSize)
{
    char *pChk = _frontPtr + iSize;
    if (_end <= pChk)
    {
        char *distance = reinterpret_cast<char *>(pChk - _end);

        _frontPtr = _begin + long(distance);
    }
    else
    {
        _frontPtr = pChk;
    }
}

