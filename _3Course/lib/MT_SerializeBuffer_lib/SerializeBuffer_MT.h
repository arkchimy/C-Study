#pragma once
#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include <Windows.h>
#include <exception>
#include <iostream>

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
    CMessage();
    ~CMessage();

    CMessage(const CMessage &) = delete;
    CMessage &operator=(const CMessage &) = delete;
    CMessage(CMessage &&) = delete;
    CMessage &operator=(CMessage &&) = delete;

    template <Fundamental T>
    CMessage &operator<<(const T &data)
    {
        if (_end < _rear + sizeof(data))
        {
            if (_size == (DWORD)en_BufferSize::bufferSize)
                ReSize();
            else
                throw MessageException(MessageException::NotEnoughSpace, "Buffer is fulled\n");
        }

        memcpy(_rear, &data, sizeof(data));
        _rear = _rear + sizeof(data);

        return *this;
    }

    template <Fundamental T>
    CMessage &operator>>(T &data)
    {
        if (_front > _rear)
            throw MessageException(MessageException::HasNotData, "false Packet \n");

        memcpy(&data, _front, sizeof(data));
        _front = _front + sizeof(data);
        return *this;
    }
    DWORD PutData(const char *src, SerializeBufferSize size);
    DWORD GetData(char *desc, SerializeBufferSize size);

    void ReSize();
    void Peek(char *out, SerializeBufferSize size);

    SerializeBufferSize _size;

    char *_begin = nullptr;
    char *_end = nullptr;

    char *_front = nullptr;
    char *_rear = nullptr;

    inline static LONG64 s_UseCnt = 0;
    inline static HANDLE s_BufferHeap = 0;
};