#include "MT_CRingBuffer_lib.h"

CRingBuffer::CRingBuffer()
    : CRingBuffer(4096) {}

CRingBuffer::CRingBuffer(ringBufferSize iBufferSize)
{
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
    return GetUseSize(_frontPtr, _rearPtr);
}

ringBufferSize CRingBuffer::GetUseSize(const char *f, const char *r)
{
    return f <= r ? r - f
                  : _end - f + r - _begin;
}

ringBufferSize CRingBuffer::GetFreeSize()
{
    return GetFreeSize(_frontPtr, _rearPtr);
}

ringBufferSize CRingBuffer::GetFreeSize(const char *f, const char *r)
{
    return f <= r ? (_end - r) + (f - _begin) - 1
                  : f - r - 1;
}

ringBufferSize CRingBuffer::Enqueue(const void *pSrc, ringBufferSize iSize)
{
    return Enqueue(pSrc,  iSize,_frontPtr,_rearPtr);
}

ringBufferSize CRingBuffer::Enqueue(const void *pSrc, ringBufferSize iSize, char *f, char *r)
{
    ringBufferSize directEnQSize, freeSize;
    ringBufferSize local_size = iSize;

    const char *chpSrc;

    chpSrc = reinterpret_cast<const char *>(pSrc);


    directEnQSize = DirectDequeueSize(f, r);
    freeSize = GetFreeSize(f, r);

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
    return Dequeue(pDest, iSize,_frontPtr,_rearPtr);
}

ringBufferSize CRingBuffer::Dequeue(void *pDest, ringBufferSize iSize, char *f, char *r)
{

    char *chpDest = reinterpret_cast<char *>(pDest);

    ringBufferSize DirectDeqSize;
    ringBufferSize useSize;
    ringBufferSize local_size;


    local_size = iSize;
    useSize = GetUseSize(f, r);

    if (useSize < local_size)
        return false;

    // TODO : 비순차적 문제는 없을까? Store라 괜찮음

    DirectDeqSize = DirectDequeueSize(f, r);

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
    return Peek(pDest, iSize, _frontPtr, _rearPtr);
}
ringBufferSize CRingBuffer::Peek(void *pDest, ringBufferSize iSize, char *f, char *r)
{
    char *chpDest = reinterpret_cast<char *>(pDest);
    ringBufferSize useSize, DirectDeqSize;


    useSize = GetUseSize(f, r);
    if (iSize > useSize)
        return useSize;
    DirectDeqSize = DirectDequeueSize(f, r);

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
    return DirectEnqueueSize(_frontPtr, _rearPtr);
}

ringBufferSize CRingBuffer::DirectEnqueueSize(const char *f, const char *r)
{
    if (f == _begin && r == _end - 1)
    {
        return 0;
    }

    return f <= r ? _end - r
                  : f - r - 1;
}

ringBufferSize CRingBuffer::DirectDequeueSize()
{
    return DirectDequeueSize(_frontPtr, _rearPtr);
}

ringBufferSize CRingBuffer::DirectDequeueSize(const char *f, const char *r)
{
    return f <= r ? r - f : _end - f;
}

void CRingBuffer::MoveRear(ringBufferSize iSize)
{

    char *pChk;
    char *distance;
    char *r; //= _rearPtr

    r = _rearPtr;
    pChk = r + iSize;
    distance = reinterpret_cast<char *>(pChk - _end);
    if (_end < pChk)
    {
        pChk = _begin + long long(distance);
    }
    InterlockedCompareExchange((unsigned long long *)&_rearPtr, (unsigned long long)pChk, (unsigned long long)r);
}

void CRingBuffer::MoveFront(ringBufferSize iSize)
{
    char *f;
    char *pChk;
    char *distance;

    f = _frontPtr;
    pChk = f + iSize;
    distance = reinterpret_cast<char *>(pChk - _end);

    if (_end < pChk)
    {
        pChk = _begin + long long(distance);
    }
    if (InterlockedCompareExchange((unsigned long long *)&_frontPtr, (unsigned long long)pChk, (unsigned long long)f) != (unsigned long long)f)
        __debugbreak();
}
