#pragma once
#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include <Windows.h>
#include <exception>
#include <iostream>

#include "../../../error_log.h"
#include "../CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"
#include <concepts>
#include <type_traits>

using SerializeBufferSize = DWORD;

#pragma pack(1)
struct stEnCordingHeader
{
    // Code(1byte) - Len(2byte) - RandKey(1byte) - CheckSum(1byte)
    BYTE Code;
    SHORT _len;
    BYTE RandKey;
    BYTE CheckSum;
};
#pragma pack()

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
                HexLog(L"SerializeBuffer_hex.txt");
            }
            else
            {
                HexLog(L"SerializeBuffer_hex.txt");
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
            HexLog(L"SerializeBuffer_hex.txt");
            throw MessageException(MessageException::HasNotData, "false Packet \n");
        }

        memcpy(&data, _frontPtr, sizeof(data));
        _frontPtr = _frontPtr + sizeof(data);
        return *this;
    }

    void EnCording();
    void DeCording();


    SSIZE_T PutData(const char *src, SerializeBufferSize size);
    SSIZE_T GetData(char *desc, SerializeBufferSize size);

    BOOL ReSize();
    void Peek(char *out, SerializeBufferSize size);

    void HexLog(const wchar_t *filename = L"SerializeBuffer_hex.txt");
    SerializeBufferSize _size;

    char *_begin = nullptr;
    char *_end = nullptr;

    char *_frontPtr = nullptr;
    char *_rearPtr = nullptr;

    ull ownerID;
    LONG64 s_UseCnt = 0;

    BYTE K = 0xa9; // 고정 키 

    inline static HANDLE s_BufferHeap;
};
