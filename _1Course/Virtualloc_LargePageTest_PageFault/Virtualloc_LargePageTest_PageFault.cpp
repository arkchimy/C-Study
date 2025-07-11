#include <Windows.h>
#include <iostream>

#include "../../../Procademy/lib/Parser_lib/Parser_lib.h"

#pragma comment(lib, "advapi32.lib")
int g_mode = 1;
int g_PageCnt = 1;

char *_current;
char *_buffer;
DWORD _seh_filter(unsigned long code, LPEXCEPTION_POINTERS info)
{
    static int s_PageFaultCnt = 0;

    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        printf("Page Access Error\n");
        //_current = _buffer;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    printf("non define Error\n");
    return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_EXECUTION;
}

struct A
{
    A()
    {
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

        size_t sizeds = GetLargePageMinimum();
        if (g_mode == 1)
        {
        realloc:
            _buffer = (char *)VirtualAlloc(nullptr, sizeds * g_PageCnt, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
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
            _buffer = (char *)VirtualAlloc(nullptr, sizeds * g_PageCnt, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (_buffer != nullptr)
                printf("VirtualAlloc Success  \n");
        }
        _current = _buffer;
    }
    ~A()
    {
        VirtualFree(_buffer, 0, MEM_RELEASE);
    }
    void Access()
    {
        __try
        {

            *_current = 'a';
            _current += 4096;
            printf("Access %p \n", _current);
        }
        __except (_seh_filter(GetExceptionCode(), GetExceptionInformation()))
        {
            printf("except _ execute \n");
        }
    }
};
int main()
{
    {
        Parser parser;
        parser.LoadFile(L"config.txt");

        parser.GetValue(L"LagePageTest", g_mode);
        parser.GetValue(L"g_PageCnt", g_PageCnt);
    }

    A *a = nullptr;
    a = new A();


    size_t sizeds = GetLargePageMinimum();

    printf("GetLargePageMinimum :   %zu \n", sizeds);

    bool bStart = false;
    while (1)
    {
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            return 0;
        }
        if (GetAsyncKeyState('a') || GetAsyncKeyState('A'))
        {
   
            bStart = true;
        }
        if (bStart)
            a->Access();
        Sleep(10);
    }
    return -1;
}
