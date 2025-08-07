#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
#pragma once


#include <Windows.h>
#include <exception>
#include <iostream>
#include "../../../error_log.h"

class MessageException : public std::exception
{
  public:
    enum ErrorType
    {
        HasNotData,
        NotEnoughSpace
    };

    MessageException(ErrorType type, const std::string &msg)
        : type_(type), msg_(msg) {}

    virtual const char *what() const noexcept override
    {
        return msg_.c_str();
    }

    ErrorType type() const noexcept { return type_; }

  private:
    ErrorType type_;
    std::string msg_;
};

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;
struct CMessage
{
    enum class en_BufferSize
    {
        bufferSize = 1000,
        MaxSize = 2000,
    };
    CMessage()
    {

        volatile LONG64 useCnt;
        DWORD exceptRetval;
        _size = (DWORD)en_BufferSize::bufferSize;

        useCnt = _InterlockedIncrement64(&s_UseCnt);
        if (useCnt == 1)
        {
            s_BufferHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
            if (s_BufferHeap == nullptr)
            {
                ERROR_FILE_LOG("HeapCreate Error\n");
                return;
            }
            else
                printf("Create Heap");
        }
        __try
        {
            _begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
            _end = _begin + _size;

            if (_begin == nullptr)
                __debugbreak(); // TODO: 종료절차로 가야할 듯.

            _rear = _begin;
            _front = _begin;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptRetval = GetExceptionCode();
            switch (exceptRetval)
            {
            case STATUS_NO_MEMORY:
                ERROR_FILE_LOG("s_BufferHeap Create Failed STATUS_NO_MEMORY \n");
                break;
            case STATUS_ACCESS_VIOLATION:
                ERROR_FILE_LOG("s_BufferHeap Create Failed STATUS_ACCESS_VIOLATION \n");
                break;
            default:
                ERROR_FILE_LOG("Not define Error \n");
            }
        }
    }
    ~CMessage()
    {
        HANDLE current_Heap;
        BOOL bHeapDeleteRetval;
        volatile LONG64 useCnt;
        current_Heap = s_BufferHeap; // s_Buffer가 덮어쓸수 있기때문에 지역으로 복사.
        useCnt = _InterlockedDecrement64(&s_UseCnt);

        if (useCnt == 0)
        {
            bHeapDeleteRetval = HeapDestroy(current_Heap);
            if (bHeapDeleteRetval == 0)
            {
                ERROR_FILE_LOG("s_BufferHeap Delete Failed \n");
                return;
            }
        }
    }
    CMessage(const CMessage &) = delete;
    CMessage &operator=(const CMessage &) = delete;
    CMessage(CMessage &&) = delete;
    CMessage &operator=(CMessage &&) = delete;

    template <Fundamental T>
    CMessage &operator<<(const T &data)
    {
        char *r = _rear;

        if (_end < r + sizeof(data))
        {
            if (_size == (DWORD)en_BufferSize::bufferSize)
                ReSize();
            else
                throw MessageException(MessageException::NotEnoughSpace, "Buffer is fulled\n");
        }

        memcpy(r, &data, sizeof(data));
        _rear = r + sizeof(data);

        return *this;
    }

    template <Fundamental T>
    CMessage &operator>>(T &data)
    {
        char *f = _front;
        if (f > _rear)
            throw MessageException(MessageException::HasNotData, "false Packet \n");

        memcpy(&data, f, sizeof(data));
        f = f + sizeof(data);
        _front = f;
        return *this;
    }
    DWORD PutData(const char *src, DWORD size)
    {
        char *r = _rear;

        if (r + size > _end)
            throw MessageException(MessageException::NotEnoughSpace, "Buffer OverFlow\n");
        memcpy(r, src, size);
        _rear += size;
        return _rear - r;
    }
    DWORD GetData(char *desc, DWORD size)
    {
        char *f = _front;
        if (f + size > _rear)
            throw MessageException(MessageException::HasNotData, "buffer has not Data\n");
        memcpy(desc, f, size);
        _front += size;
        return _front - f;
    }
    void ReSize() {} // 미구현
    void Peek(char *out, DWORD size)
    {
        char *f = _front;
        if (f + size > _rear)
            throw MessageException(MessageException::HasNotData, "buffer has not Data\n");
        memcpy(out, f, size);
    }
    DWORD _size;

    char *_begin = nullptr;
    char *_end = nullptr;

    char *_front = nullptr;
    char *_rear = nullptr;

    inline static LONG64 s_UseCnt = 0;
    inline static HANDLE s_BufferHeap = 0;
};