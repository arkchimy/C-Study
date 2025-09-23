#include "CLargePage.h"
#include <Windows.h>
#include <iostream>

CLargePage::CLargePage()
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
    // AdjustTokenPrivileges(GetCurrentProcess(), FALSE, SE_PRIVILEGE_ENABLED,)
    LUID PrivilegeRequired;
    BOOL bRes = FALSE;

    bRes = LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &PrivilegeRequired);

    size_t sizeds = GetLargePageMinimum();


    virtualMemoryBegin = (char *)VirtualAlloc(nullptr, sizeds , MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (GetLastError() != 0)
    {
        printf("VirtualAlloc Error  %d \n", GetLastError());
        Sleep(5000);
    }
    else
    {
        printf("LargePage VirtualAlloc Success  \n");
    }
    
}
char *CLargePage::Alloc(unsigned long long size, bool bLargePage)
{
    static unsigned long long cnt = 0;
    if (bLargePage == false)
        return (char*)malloc(size);

    return virtualMemoryBegin + size * cnt++;
}
CLargePage& CLargePage::GetInstance()
{
    static CLargePage instance;
    return instance;
}

