#pragma once
#include <Windows.h>
#include <iostream>

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;

struct CMessage
{
    enum class en_ErrorType
    {
        hasNotData, // 빼려고하는데 데이터가 없다.
        notenouthSpace,
        MAX,
    };
    enum class en_BufferSize
    {
        bufferSize = 1000,
        MaxSize = 2000,
    };
    CMessage()
    {
        static volatile long long oneChk = 0;
        if (oneChk == 0)
        {
            if (InterlockedExchange64(&oneChk, 1) == 0)
            {
                s_BufferHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
                if (s_BufferHeap == nullptr)
                {
                    printf("s_BufferHeap Create Failed \n");
                    __debugbreak();
                }
            }
        }

        _size = (DWORD)en_BufferSize::bufferSize;
        _begin = (char *)HeapAlloc(s_BufferHeap, 0, _size);
        _end = _begin + _size;

        if (_begin == nullptr)
            __debugbreak();


        _rear = _begin;
        _front = _begin;

        ZeroMemory(_begin, _size);

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

    template <Fundamental T>
    CMessage &operator<<(const T &data)
    {
        if (_end < _rear + sizeof(data))
            throw en_ErrorType::notenouthSpace;

        memcpy(_rear, &data, sizeof(data));
        _rear += sizeof(data);

        return *this;
    }

    template <Fundamental T>
    CMessage &operator>>(T &data)
    {
        memcpy(&data, _front, sizeof(data));
        _front += sizeof(data);

        if (_front > _rear)
            throw en_ErrorType::hasNotData;
        return *this;
    }
    DWORD _size;

    char *_begin, *_end;
    char *_rear, *_front;

    static HANDLE s_CMessageHeap;
    static HANDLE s_BufferHeap;
};