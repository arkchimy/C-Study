#pragma once
#include <Windows.h>
#include <iostream>

struct CMessage
{
    CMessage()
    {
        _rear = _buffer;
        _front = _buffer;

        _begin = _buffer;
        _end = _buffer;
        ZeroMemory(_buffer, sizeof(_buffer));
    }
    void *operator new(size_t size)
    {
        static volatile long long oneChk = 0;
        if (oneChk == 0)
        {
            if (InterlockedExchange64(&oneChk, 1) == 0)
            {
                s_CMessageHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
                if (s_CMessageHeap == nullptr)
                {
                    printf("s_CMessageHeap Create Failed \n");
                    __debugbreak();
                }
            }
        }
        return HeapAlloc(s_CMessageHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, sizeof(CMessage));
    }
    template <typename T>
    CMessage &operator<<(const T &data)
    {
        memcpy(_rear, &data, sizeof(data));
        _rear += sizeof(data);
        return *this;
    }
    template <typename T>
    CMessage &operator>>(T &data)
    {
        memcpy(&data, _front, sizeof(data));
        _front += sizeof(data);

        return *this;
    }

    char _buffer[1000];
    char *_begin, *_end;
    char *_rear, *_front;

    static HANDLE s_CMessageHeap;
};