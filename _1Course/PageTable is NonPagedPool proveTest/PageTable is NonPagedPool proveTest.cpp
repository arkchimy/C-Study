
#include <iostream>
#include <windows.h>

struct stBigBuffer
{
    stBigBuffer()
    {
        _buffer = (char*)VirtualAlloc(nullptr, 4096 * 1024 *300 , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (_buffer == nullptr)
            __debugbreak();
        _limit = _buffer + 4096 * 1024 * 300;
        _current = _buffer;
    }
    void Test()
    {
        _current += 100;
        Sleep(10);
        if (_current == _limit)
        {
            _current = _buffer;
        }
        _current[0] = 'a';
    }
    char *_buffer;
    char *_limit;
    char *_current = nullptr;
};
int main()
{
    stBigBuffer buffer;
    while (1)
    {
        buffer.Test();
    }

}
