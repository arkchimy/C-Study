#include "MT_CRingBuffer_lib.h"

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

ringBufferSize CRingBuffer::GetUseSize()
{
    char *f = _frontPtr;
    char *r = _rearPtr;

    return f <= r ? r - f
                  : _end - f + r - _begin;
}

ringBufferSize CRingBuffer::GetFreeSize()
{
    // 가득 채우면 안되므로 -1
    char *f = _frontPtr;
    char *r = _rearPtr;

    return f <= r ? (_end - r) + (f - _begin) - 1
                  : f - r - 1;
}



bool CRingBuffer::Enqueue(const void *pSrc, ringBufferSize iSize)
{
    ringBufferSize directEnQSize, freeSize;
    char *f = _frontPtr, *r = _rearPtr;
    const char *chpSrc = reinterpret_cast<const char *>(pSrc);

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

bool CRingBuffer::Dequeue(void *pDest, ringBufferSize iSize)
{
    char *f, *r;
    char *chpDest = reinterpret_cast<char *>(pDest);

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

ringBufferSize CRingBuffer::Peek(void *pDest, ringBufferSize iSize)
{
    char *chpDest = reinterpret_cast<char*>(pDest);
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

void CRingBuffer::ClearBuffer()
{
    _rearPtr = _begin;
    _frontPtr = _begin;
}

ringBufferSize CRingBuffer::DirectEnqueueSize()
{
    char *f = _frontPtr, *r = _rearPtr;

    if (f == _begin && r == _end)
    {
        return _end - r - 1;
    }

    return f <= r ? _end - r
                  : f - r - 1;
}

ringBufferSize CRingBuffer::DirectDequeueSize()
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
