// SerializeBuffer_MT.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include <strsafe.h>
#include "../../../_1Course/lib/Parser_lib/Parser_lib.h"

#define ERROR_BUFFER_SIZE 100

static int g_mode = 0;

CMessage::CMessage()
{

    DWORD exceptRetval;
    _size = en_BufferSize::bufferSize;

    static HANDLE Allcoheap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    s_BufferHeap = Allcoheap;

    _begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
    if (_begin == nullptr)
        __debugbreak();
    _end = _begin + _size;
    _frontPtr = _begin;
    _rearPtr = _frontPtr;
}

CMessage::~CMessage()
{
    HeapFree(s_BufferHeap, 0, _begin);
    _begin = nullptr;

}

SSIZE_T CMessage::PutData(const char *src, SerializeBufferSize size)
{
    char *r = _rearPtr;
    if (r + size > _end)
        throw MessageException(MessageException::NotEnoughSpace, "Buffer OverFlow\n");
    memcpy(r, src, size);
    _rearPtr += size;
    return _rearPtr - r;
}

SSIZE_T CMessage::GetData(char *desc, SerializeBufferSize size)
{
    char *f = _frontPtr;
    if (f + size > _rearPtr)
    {
        throw MessageException(MessageException::HasNotData, "buffer has not Data\n");
    }
    memcpy(desc, f, size);
    _frontPtr += size;
    return _frontPtr - f;
}

// 지금 까지의 모든 데이터를 새로 할당받은 메모리에 복사후 그대로 진행해야 함.
BOOL CMessage::ReSize()
{
    // 직렬화 버퍼는 넣고 뺴고는 하나의 쓰레드에서 할 것으로 예상이 된다.

    SerializeBufferSize UseSize;
    char *swap_begin;

    UseSize = SerializeBufferSize(_rearPtr - _frontPtr);

    _size = en_BufferSize::MaxSize;
    swap_begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
    if (swap_begin == nullptr)
    {
        ERROR_FILE_LOG(L"HeapAlloc.txt", L"HeapAlloc Error");
        return FALSE;
    }
    // TODO : 복사 범위 생각해보기.
    memcpy(swap_begin, _frontPtr, UseSize);
    HeapFree(s_BufferHeap, 0, _begin);

    _begin = swap_begin;
    _end = swap_begin + _size;
    _frontPtr = _begin;
    _rearPtr = _begin + UseSize;
    printf("ReSize\n");
    return TRUE;
}

void CMessage::Peek(char *out, SerializeBufferSize size)
{
    char *f = _frontPtr;
    if (f + size > _rearPtr)
        throw MessageException(MessageException::HasNotData, "buffer has not Data\n");
    memcpy(out, f, size);
}
