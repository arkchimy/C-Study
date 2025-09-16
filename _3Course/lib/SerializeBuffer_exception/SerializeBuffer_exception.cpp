// SerializeBuffer_MT.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include <strsafe.h>
#include "../../../_1Course/lib/Parser_lib/Parser_lib.h"

#define ERROR_BUFFER_SIZE 100

static CObjectPoolManager instance;
static int g_mode = 0;

CMessage::CMessage()
{

    volatile LONG64 useCnt;
    DWORD exceptRetval;
    _size = en_BufferSize::bufferSize;

    useCnt = InterlockedIncrement64(&s_UseCnt);

    static HANDLE Allcoheap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    s_BufferHeap = Allcoheap;
    __try
    {
        if (g_mode == 0)
        {
            _begin = (char *)HeapAlloc(s_BufferHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, _size);
            if (_begin == nullptr)
                __debugbreak();
        }
        else
        {
            _begin = CObjectPoolManager::Alloc();
        }
        _end = _begin + _size;

        _rearPtr = _begin;
        _frontPtr = _begin;
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
    // if (useCnt == 0)
    //{
    //     bHeapDeleteRetval = HeapDestroy(current_Heap);
    //     if (bHeapDeleteRetval == 0)
    //     {
    //         ERROR_FILE_LOG(L"SerializeError.txt", L"s_BufferHeap Delete Failed ");
    //     }
    //     else
    //         printf("heap delete");
    // }
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

    UseSize = _rearPtr - _frontPtr;

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

CObjectPoolManager::CObjectPoolManager()
{
    char *virtualMemoryBegin;

    int g_PageCnt = 0;
    {
        Parser parser;
        parser.LoadFile(L"Config.txt");

        parser.GetValue(L"LagePageTest", g_mode);
        parser.GetValue(L"g_PageCnt", g_PageCnt);
        parser.GetValue(L"g_PageCnt", g_PageCnt);
    }

    TOKEN_PRIVILEGES tp;
    HANDLE hToken;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid))
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError());

        return;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
    if (!AdjustTokenPrivileges(
            hToken,                   // TokenHandle
            FALSE,                    // DisableAllPrivileges: FALSE면 지정 권한만 변경
            &tp,                      // NewState: 활성화할 권한 정보
            sizeof(TOKEN_PRIVILEGES), // BufferLength
            NULL,                     // PreviousState: 이전 상태 필요 없으면 NULL
            NULL                      // ReturnLength: 필요 없으면 NULL
            ))
        ;
    // AdjustTokenPrivileges(GetCurrentProcess(), FALSE, SE_PRIVILEGE_ENABLED,)
    LUID PrivilegeRequired;
    BOOL bRes = FALSE;

    bRes = LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &PrivilegeRequired);

    size_t sizeds = GetLargePageMinimum();
    if (g_mode == 1)
    {
    realloc:
        virtualMemoryBegin = (char *)VirtualAlloc(nullptr, sizeds * g_PageCnt, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
        if (GetLastError() != 0)
        {
            printf("VirtualAlloc Error  %d \n", GetLastError());
            Sleep(5000);
            goto realloc;
        }
        else
        {
            printf("LargePage VirtualAlloc Success  \n");
        }
    }
    else
    {
        virtualMemoryBegin = (char *)VirtualAlloc(nullptr, sizeds * g_PageCnt, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (virtualMemoryBegin != nullptr)
            printf("VirtualAlloc Success  \n");
    }
    InterlockedExchange((ull *)&CObjectPoolManager::_buffer, (ull)virtualMemoryBegin);
}
char *CObjectPoolManager::Alloc()
{


    char *pRetval;

    pRetval = _buffer + cnt * CMessage::en_BufferSize::bufferSize;
    *pRetval = 0;
    cnt++;

    return pRetval;
}