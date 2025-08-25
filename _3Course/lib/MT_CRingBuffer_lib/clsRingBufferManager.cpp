#include "clsRingBufferManager.h"
#include "../../../_1Course/lib/Parser_lib/Parser_lib.h"

using ull = unsigned long long;

clsRingBufferManager::clsRingBufferManager()
{
    char *virtualMemoryBegin;
    int g_mode = 0;
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
    InterlockedExchange((ull *)&_buffer, (ull)virtualMemoryBegin);

}

char *clsRingBufferManager::Alloc(int iSize)
{
    static ull cnt = -1;
    ull current;
    current = InterlockedIncrement(&cnt);

    return _buffer + iSize * current;
}

clsRingBufferManager *clsRingBufferManager::GetInstance()
{
    static clsRingBufferManager instance;

    return &instance;
}
