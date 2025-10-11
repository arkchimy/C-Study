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
    AdjustTokenPrivileges(
        hToken,                   // TokenHandle
        FALSE,                    // DisableAllPrivileges: FALSE면 지정 권한만 변경
        &tp,                      // NewState: 활성화할 권한 정보
        sizeof(TOKEN_PRIVILEGES), // BufferLength
        NULL,                     // PreviousState: 이전 상태 필요 없으면 NULL
        NULL                      // ReturnLength: 필요 없으면 NULL
    );
    // AdjustTokenPrivileges(GetCurrentProcess(), FALSE, SE_PRIVILEGE_ENABLED,)
    LUID PrivilegeRequired;
    BOOL bRes = FALSE;

    bRes = LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &PrivilegeRequired);


    virtualMemoryBegin = (char *)VirtualAlloc(nullptr, 4096 * g_PageCnt, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    virtualMemoryEnd = virtualMemoryBegin + 4000 * g_PageCnt;

    if (virtualMemoryBegin == nullptr)
    {
        printf("VirtualAlloc Failed  \n");
        __debugbreak();
    }
    printf("VirtualAlloc Success  \n");
    
    InterlockedExchange((ull *)&_buffer, (ull)virtualMemoryBegin);
}

char *clsRingBufferManager::Alloc(int iSize)
{
    static ull cnt = -1;
    ull current;
    current = InterlockedIncrement(&cnt);

    
    return virtualMemoryEnd <= _buffer + iSize * current ? nullptr : _buffer + iSize * current;
}

clsRingBufferManager *clsRingBufferManager::GetInstance()
{
    static clsRingBufferManager instance;

    return &instance;
}
