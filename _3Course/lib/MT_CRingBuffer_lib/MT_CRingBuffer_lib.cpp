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

ringBufferSize CRingBuffer::GetUseSize(const char *f, const char *r)
{
    return f <= r ? r - f
                  : _end - f + r - _begin;
}

ringBufferSize CRingBuffer::GetFreeSize()
{
    // 가득 채우면 안되므로 -1
    char *f = _frontPtr;
    char *r = _rearPtr;

    return GetFreeSize(f, r);
}

ringBufferSize CRingBuffer::GetFreeSize(const char *f, const char *r)
{
    return f <= r ? (_end - r) + (f - _begin) - 1
                  : f - r - 1;
}



ringBufferSize CRingBuffer::Enqueue(const void *pSrc, ringBufferSize iSize)
{
    ringBufferSize directEnQSize, freeSize;
    ringBufferSize local_size = iSize;

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

    if (freeSize < local_size)
    {
        return false;
    }

    if (local_size <= directEnQSize)
    {
        memcpy(r, chpSrc, local_size);
    }
    else
    {
        memcpy(r, chpSrc, directEnQSize);
        memcpy(_begin, chpSrc + directEnQSize, local_size - directEnQSize);
    }
    MoveRear(local_size);

    return local_size;
}

ringBufferSize CRingBuffer::Dequeue(void *pDest, ringBufferSize iSize)
{
    char *f, *r;
    char *chpDest = reinterpret_cast<char *>(pDest);

    ringBufferSize DirectDeqSize;
    ringBufferSize useSize;
    ringBufferSize local_size = iSize;

    f = _frontPtr;
    r = _rearPtr;

    useSize = f <= r ? r - f
                     : _end - f + r - _begin;

    if (useSize < local_size)
        return false;

    // TODO : 비순차적 문제는 없을까? Store라 괜찮음

    DirectDeqSize = f <= r ? r - f : _end - f;

    if (local_size <= DirectDeqSize)
    {
        memcpy(chpDest, f, local_size);
    }
    else
    {
        memcpy(chpDest, f, DirectDeqSize);
        memcpy(chpDest + DirectDeqSize, _begin, local_size - DirectDeqSize);
    }
    MoveFront(local_size);

    return local_size;
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

ringBufferSize CRingBuffer::DirectEnqueueSize(const char *f, const char *r)
{
    if (f == _begin && r  == _end - 1)
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

ringBufferSize CRingBuffer::DirectDequeueSize(const char *f, const char *r)
{
    return f <= r ? r - f : _end - f;
}

void CRingBuffer::MoveRear(ringBufferSize iSize)
{
    char *pChk = _rearPtr + iSize;
    char *distance = reinterpret_cast<char *>(pChk - _end);

    if (_end < pChk)
    {
        _rearPtr = _begin + long long(distance);
    }
    else
    {
        _rearPtr = pChk;
    }
}

void CRingBuffer::MoveFront(ringBufferSize iSize)
{
    char *pChk = _frontPtr + iSize;
    char *distance = reinterpret_cast<char *>(pChk - _end);

    if (_end < pChk)
    {
    
        _frontPtr = _begin + long long(distance);
    }
    else
    {
        _frontPtr = pChk;
    }
}
