// SerializeBuffer_MT.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//
#include "pch.h"
#include <strsafe.h>
#define ERROR_BUFFER_SIZE 100

 static HANDLE s_BufferHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);

CMessage::CMessage()
{

    volatile LONG64 useCnt;
    DWORD exceptRetval;
    _size = en_BufferSize::bufferSize;

    useCnt = InterlockedIncrement64(&s_UseCnt);
    //if (useCnt == 1)
    //{
    //    s_BufferHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    //    if (s_BufferHeap == nullptr)
    //    {
    //        ERROR_FILE_LOG(L"SerializeError.txt", L"HeapCreate Error\n");
    //        return;
    //    }
    //}

    __try
    {
        _begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
        _end = _begin + _size;

        _rear = _begin;
        _front = _begin;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        exceptRetval = GetExceptionCode();
        switch (exceptRetval)
        {
        case STATUS_NO_MEMORY:
            ERROR_FILE_LOG(L"SerializeError.txt", L"s_BufferHeap Create Failed STATUS_NO_MEMORY \n");
            break;
        case STATUS_ACCESS_VIOLATION:
            ERROR_FILE_LOG(L"SerializeError.txt", L"s_BufferHeap Create Failed STATUS_ACCESS_VIOLATION \n");
            break;
        default:
            ERROR_FILE_LOG(L"SerializeError.txt", L"Not define Error \n");
        }
    }
}

CMessage::~CMessage()
{
    HANDLE current_Heap;
    BOOL bHeapDeleteRetval;
    volatile LONG64 useCnt;

    current_Heap = s_BufferHeap; // s_Buffer가 덮어쓸수 있기때문에 지역으로 복사.
    useCnt = InterlockedDecrement64(&s_UseCnt);

    HeapFree(current_Heap, 0, _begin);
    _begin = nullptr;
    //if (useCnt == 0)
    //{
    //    bHeapDeleteRetval = HeapDestroy(current_Heap);
    //    if (bHeapDeleteRetval == 0)
    //    {
    //        ERROR_FILE_LOG(L"SerializeError.txt", L"s_BufferHeap Delete Failed ");
    //    }
    //    else
    //        printf("heap delete");
    //}
}

SSIZE_T CMessage::PutData(const char *src, SerializeBufferSize size)
{
    char *r = _rear;
    if (r + size > _end)
        throw MessageException(MessageException::NotEnoughSpace, "Buffer OverFlow\n");
    memcpy(r, src, size);
    _rear += size;
    return _rear - r;
}

SSIZE_T CMessage::GetData(char *desc, SerializeBufferSize size)
{
    char *f = _front;
    if (f + size > _rear)
    {
        throw MessageException(MessageException::HasNotData, "buffer has not Data\n");
    }
    memcpy(desc, f, size);
    _front += size;
    return _front - f;
}

// 지금 까지의 모든 데이터를 새로 할당받은 메모리에 복사후 그대로 진행해야 함.
BOOL CMessage::ReSize()
{
    // 직렬화 버퍼는 넣고 뺴고는 하나의 쓰레드에서 할 것으로 예상이 된다.

    SerializeBufferSize UseSize;
    char *swap_begin;

    UseSize = _rear - _front;

    _size = en_BufferSize::MaxSize;
    swap_begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
    if (swap_begin == nullptr)
    {
        ERROR_FILE_LOG(L"HeapAlloc.txt", L"HeapAlloc Error");
        return FALSE;
    }
    // TODO : 복사 범위 생각해보기.
    memcpy(swap_begin, _front, UseSize);
    HeapFree(s_BufferHeap, 0, _begin);

    _begin = swap_begin;
    _end = swap_begin + _size;
    _front = _begin;
    _rear = _begin + UseSize;
    printf("ReSize\n");
    return TRUE;
}

void CMessage::Peek(char *out, SerializeBufferSize size)
{
    char *f = _front;
    if (f + size > _rear)
        throw MessageException(MessageException::HasNotData, "buffer has not Data\n");
    memcpy(out, f, size);
}
