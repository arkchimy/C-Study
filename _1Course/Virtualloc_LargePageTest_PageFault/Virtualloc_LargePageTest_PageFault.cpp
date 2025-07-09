
//
//#include <iostream>
//#include <windows.h>
//
//int main()
//{
//    // Large Page 크기 확인
//    SIZE_T largePageSize = GetLargePageMinimum();
//    if (largePageSize == 0)
//    {
//        std::cerr << "Large pages not supported on this system." << std::endl;
//        return 1;
//    }
//
//    // 할당할 메모리 크기 (largePageSize의 배수여야 함)
//    SIZE_T allocSize = largePageSize * 1; // 예: 2MB
//
//    // VirtualAlloc을 사용해 Large Page 메모리 할당
//    void *lpMem = VirtualAlloc(
//        NULL,
//        allocSize,
//        MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
//        PAGE_READWRITE);
//
//    if (lpMem == NULL)
//    {
//        std::cerr << "VirtualAlloc failed with error: " << GetLastError() << std::endl;
//        return 1;
//    }
//
//    std::cout << "Large page memory allocated at: " << lpMem << std::endl;
//
//    // 메모리 해제
//    VirtualFree(lpMem, 0, MEM_RELEASE);
//
//    return 0;
//}


// Virtualloc_LargePageTest_PageFault.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <Windows.h>
#include <iostream>

//#include "C:/Procamy/lib/Parser_lib/Parser_lib.h"
#include "../../../Procademy/lib/Parser_lib/Parser_lib.h"

#pragma comment(lib, "advapi32.lib")
int g_mode;

char *_current;
char *_buffer;
DWORD _seh_filter(unsigned long code, LPEXCEPTION_POINTERS info)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        printf("Page Access Error\n");
        _current = _buffer;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    printf("non define Error\n");
    return EXCEPTION_EXECUTE_HANDLER;
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
        if (g_mode)
        {
            _buffer = (char *)VirtualAlloc(nullptr, sizeds , MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
            if (GetLastError() != 0)
            {
                printf("VirtualAlloc Error  %d \n", GetLastError());
                Sleep(10000);
            }
        }
        else
        {
            _buffer = (char *)VirtualAlloc(nullptr, sizeds , MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }
        _current = _buffer;
    }
    ~A()
    {
    }
    void Access()
    {
        __try
        {
            *_current = 'a';
            _current += 100;
        }
        __except (_seh_filter(GetExceptionCode(), GetExceptionInformation()))
        {
            _current = _buffer;
        }
    }

};
int main()
{
    Parser parser;
    parser.LoadFile(L"config.txt");

    parser.GetValue(L"라지페이지On", g_mode);

    A a;

    while (1)
    {
        if (GetAsyncKeyState(VK_ESCAPE))
        {
            return 0;
        }
        a.Access();
        Sleep(100);
    }
    return -1;
}
