#pragma once
#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include <Windows.h>
#include <exception>
#include <iostream>

#include "../../../error_log.h"
#include "../CLockFreeMemoryPool/CLockFreeMemoryPool.h"
#include <concepts>
#include <type_traits>

using SerializeBufferSize = DWORD;
class MessageException : public std::exception
{
  public:
    enum ErrorType
    {
        HasNotData,
        NotEnoughSpace
    };

    MessageException(ErrorType type, const std::string &msg)
        : _type(type), _msg(msg) {}

    virtual const char *what() const noexcept override
    {
        return _msg.c_str();
    }

    ErrorType type() const noexcept { return _type; }

  private:
    ErrorType _type;
    std::string _msg;
};

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;
struct CMessage
{
    enum en_BufferSize : DWORD
    {
        bufferSize = 200,
        MaxSize = 2000,
    };
    CMessage();
    ~CMessage();
    CMessage(const CMessage &) = delete;
    CMessage &operator=(const CMessage &) = delete;
    CMessage(CMessage &&) = delete;
    CMessage &operator=(CMessage &&) = delete;

    template <Fundamental T>
    CMessage &operator<<(const T data)
    {
        if (_end < _rearPtr + sizeof(data))
        {
            if (_size == en_BufferSize::bufferSize)
            {
                ReSize();
                HEX_FILE_LOG(L"SerializeBuffer_hex.txt", _begin, _size);
            }
            else
            {
                HEX_FILE_LOG(L"SerializeBuffer_hex.txt", _begin, _size);
                throw MessageException(MessageException::NotEnoughSpace, "Buffer is fulled\n");
            }
        }

        memcpy(_rearPtr, &data, sizeof(data));
        _rearPtr = _rearPtr + sizeof(data);

        return *this;
    }

    template <Fundamental T>
    CMessage &operator>>(T &data)
    {
        if (_frontPtr > _rearPtr)
        {
            HEX_FILE_LOG(L"SerializeBuffer_hex.txt", _begin, _size);
            throw MessageException(MessageException::HasNotData, "false Packet \n");
        }

        memcpy(&data, _frontPtr, sizeof(data));
        _frontPtr = _frontPtr + sizeof(data);
        return *this;
    }
    SSIZE_T PutData(const char *src, SerializeBufferSize size);
    SSIZE_T GetData(char *desc, SerializeBufferSize size);

    BOOL ReSize();
    void Peek(char *out, SerializeBufferSize size);

    SerializeBufferSize _size;

    char *_begin = nullptr;
    char *_end = nullptr;

    char *_frontPtr = nullptr;
    char *_rearPtr = nullptr;

    inline static LONG64 s_UseCnt = 0;

    friend class CObjectPoolManager;
    inline static HANDLE s_BufferHeap;
};
struct CObjectPoolManager
{
    inline static class CObjectPool<CMessage> pool;
    inline static char *_buffer;
    inline static int cnt = 0;
    CObjectPoolManager();
    static char* Alloc();
};
