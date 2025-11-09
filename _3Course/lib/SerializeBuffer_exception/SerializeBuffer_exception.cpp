// SerializeBuffer_MT.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include <strsafe.h>
#include "../../../_1Course/lib/Parser_lib/Parser_lib.h"


static int g_mode = 0;

CMessage::CMessage()
{

    _size = en_BufferSize::bufferSize;

    static HANDLE Allcoheap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    s_BufferHeap = Allcoheap;

    _begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
    if (_begin == nullptr)
        __debugbreak();
    _end = _begin + _size;
    _frontPtr = _begin ;
    _rearPtr = _frontPtr;
}

CMessage::~CMessage()
{
    HeapFree(s_BufferHeap, 0, _begin);
    _begin = nullptr;

}

void CMessage::EnCoding( )
{
    SerializeBufferSize len;
    BYTE RK;
    BYTE total = 0;
    int current = 0;

    BYTE P = 0;
    BYTE E = 0;
    char *local_Front;

    RK = 0x31;

    local_Front = _begin + offsetof(stEnCordingHeader, CheckSum);
    len = _rearPtr - local_Front;
    
    for (int i = 0; i < len; i++)
    {
        total += local_Front[i];
    }
    memcpy(local_Front, &total, sizeof(total));

    for (; &local_Front[current] != _rearPtr; current++)
    {
        BYTE D1 = (local_Front)[current];
        BYTE b = (P + RK + current);

        P = D1 ^ b;
        E = P ^ (E + K + current);
        local_Front[current] = E;
    }

}

void CMessage::DeCoding( )
{
    BYTE P1 = 0, P2;
    BYTE E1 = 0, E2;
    BYTE D1 = 0, D2;
    BYTE total = 0;
    BYTE RK;
    char *local_Front;

    // 디코딩의 msg는 링버퍼에서 꺼낸 데이터로 내가 작성하는
    SerializeBufferSize len;
    int current = 0;

    RK = *(_begin + offsetof(stEnCordingHeader, RandKey));

    HexLog();
    local_Front = _begin + offsetof(stEnCordingHeader, CheckSum);
    len = _rearPtr - local_Front;

    // 2기준
    // D2 ^ (P1 + RK + 2) = P2
    // P2 ^ (E1 + K + 2) = E2

    // E2 ^ (E1 + K + 2) = P2
    // P2 ^ (P1 + RK + 2) = D2
    for (; &local_Front[current] != _rearPtr; current++)
    {
        E2 = local_Front[current];
        P2 = E2 ^ (E1 + K + current);
        E1 = E2;
        D2 = P2 ^ (P1 + RK + current);
        P1 = P2;
        local_Front[current] = D2;
    }

    for (int i = 1; i < len; i++)
    {
        total += local_Front[i];
    }

    memcpy(local_Front, &total, sizeof(total));

    HexLog();
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

void CMessage::HexLog(const wchar_t *filename)
{
    int current = 0;

    wchar_t hexBuffer[MaxSize * 3 + 1]; // 최대 바이트와 띄어쓰기, 널문자 까지 포함. 
    wchar_t wordSet[] = L"0123456789ABCDEF";
    BYTE data;

    for (; &_begin[current] != _end; current++)
    {
        data = _begin[current];

        hexBuffer[3 * current + 0] = wordSet[ data  >> 4];
        hexBuffer[3 * current + 1] = wordSet[ data & 0xF];
        hexBuffer[3 * current + 2] = L' ';
    }
    hexBuffer[3 * current + 0] = L'\n';
    hexBuffer[3 * current + 1] = L'\n';
    FILE* file;
    file = nullptr;
    while (file == nullptr)
    {
        _wfopen_s(&file, filename, L"a+ ,ccs=UTF-16LE");
    }
    fwrite(hexBuffer, 2, current * 3 + 1, file);
    current = 0;
    for (; &_begin[current] != _end; current++)
    {
        hexBuffer[3 * current + 0] = L' ';
        hexBuffer[3 * current + 1] = L' ';
        hexBuffer[3 * current + 2] = L' ';
    }

    hexBuffer[3 * (_frontPtr - _begin)] = L'F';
    hexBuffer[3 * (_rearPtr - _begin)] = L'R';
    fwrite(hexBuffer, 2, current * 3 + 1, file);

    fclose(file);

}

