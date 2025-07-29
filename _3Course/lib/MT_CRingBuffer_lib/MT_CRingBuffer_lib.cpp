// MT_CRingBuffer_lib.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "framework.h"
#include "pch.h"

#include <memory>

CRingBuffer::CRingBuffer()
{
    _begin = (char *)_aligned_malloc(18, 4096);
    _end = _begin + 18;
    ClearBuffer();
}

CRingBuffer::CRingBuffer(ringBufferSize iBufferSize)
{
    /*  ringBufferSize realSize = iBufferSize % 4096;
      realSize = realSize == 0 ? iBufferSize : iBufferSize + realSize;*/

    _begin = (char *)_aligned_malloc(iBufferSize, 4096);
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

    // TODO : 이게 멀쓰에서 안전한가?
    /* if (f <= r)
     {
         _end += _end - _begin;

     }*/

    return false;
}

bool CRingBuffer::Enqueue(const char *chpSrc, ringBufferSize iSize)
{
    ringBufferSize directEnQSize, freeSize;
    char *f = _frontPtr, *r = _rearPtr;

    if (f == _begin && r == _end)
    {
        directEnQSize = _end - r - 1;
    }
    else
    {
        directEnQSize = f <= r ? _end - r
                               : f - r - 1;
    }
    freeSize = f <= r ? (_end - r) + (f - _begin) - 1
                      : f - r - 1;

    if (freeSize <= iSize)
    {
        bool bChk = ReSize();
        if (bChk == false)
            return false;
    }

    if (iSize <= directEnQSize)
    {
        memcpy(r, chpSrc, iSize);
    }
    else
    {
        memcpy(r, chpSrc, directEnQSize);
        memcpy(_begin, chpSrc + directEnQSize, iSize - directEnQSize);
    }
    MoveRear(iSize);

    return true;
}

bool CRingBuffer::Dequeue(char *chpDest, ringBufferSize iSize)
{
    char *f, *r;
    ringBufferSize DirectDeqSize;
    ringBufferSize useSize;
    f = _frontPtr;
    r = _rearPtr;

    useSize = f <= r ? r - f
                     : _end - f + r - _begin;

    if (useSize < iSize)
        return false;

    // TODO : 비순차적 문제는 없을까? Store라 괜찮음

    DirectDeqSize = f <= r ? r - f : _end - f;

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
    char *f = _frontPtr, *r = _rearPtr;
    ringBufferSize useSize, DirectDeqSize;

    useSize = f <= r ? r - f
                     : _end - f + r - _begin;

    if (iSize > useSize)
        return useSize;

    DirectDeqSize = f <= r ? r - f : _end - f;

    if (iSize <= DirectDeqSize)
    {
        memcpy(chpDest, f, iSize);
    }
    else
    {
        memcpy(chpDest, f, DirectDeqSize);
        memcpy(chpDest + DirectDeqSize, _begin, iSize - DirectDeqSize);
    }
    return iSize;
}

void CRingBuffer::ClearBuffer(void)
{
    _rearPtr = _begin;
    _frontPtr = _begin;
}

ringBufferSize CRingBuffer::DirectEnqueueSize(void)
{
    char *f = _frontPtr, *r = _rearPtr;

    if (f == _begin && r == _end)
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
    char *pChk = _rearPtr + iSize;
    char *distance = reinterpret_cast<char *>(pChk - _end);

    if (_end < pChk)
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
    if (_end < pChk)
    {
        char *distance = reinterpret_cast<char *>(pChk - _end);

        _frontPtr = _begin + long(distance);
    }
    else
    {
        _frontPtr = pChk;
    }
}
