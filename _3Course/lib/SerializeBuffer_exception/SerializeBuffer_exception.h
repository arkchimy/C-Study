#pragma once
#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include <Windows.h>
#include <exception>
#include <iostream>

#include "../../../error_log.h"
#include "../CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"
#include <concepts>
#include <type_traits>
#include "../CNetwork_lib/stHeader.h"

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
        bufferSize = 1000,
        MaxSize = 2000,
    };
    enum class en_Tag : BYTE
    {
        ENCODE,
        DECODE,
        NORMAL,
        _ERROR,
        MAX,
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
                HexLog(en_Tag::_ERROR);
            }
            else
            {
                HexLog(en_Tag::_ERROR);
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
            HexLog(en_Tag::_ERROR);
            throw MessageException(MessageException::HasNotData, "false Packet \n");
        }

        memcpy(&data, _frontPtr, sizeof(data));
        _frontPtr = _frontPtr + sizeof(data);
        return *this;
    }

    void EnCoding();
    bool DeCoding();


    SSIZE_T PutData(PVOID src, SerializeBufferSize size);
    SSIZE_T GetData(char *desc, SerializeBufferSize size);

    BOOL ReSize();
    void Peek(char *out, SerializeBufferSize size);

    void HexLog(en_Tag tag = en_Tag::NORMAL, const wchar_t *filename = L"SerializeBuffer_hex.txt");
    SerializeBufferSize _size = en_BufferSize::bufferSize;

    char _begin[MaxSize];
    char *_end = nullptr;

    char *_frontPtr = nullptr;
    char *_rearPtr = nullptr;

    ull ownerID;
    LONG64 iUseCnt = 0;

    BYTE K = 0xa9; // 고정 키 
  
    inline static HANDLE s_BufferHeap;
};
