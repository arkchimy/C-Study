#pragma once

#define WIN32_LEAN_AND_MEAN
#include <iostream>

#include <Windows.h>
#include <conio.h>
#include <mutex>
#include <thread>

#include <map>
#include <strsafe.h>
#include <unordered_set>
struct stTlsLockInfo;

class MyMutexManager
{
  private:
    MyMutexManager() = default;

  public:
    static MyMutexManager *GetInstance()
    {
        // 함수가 실패하면 반환 값은 NULL.
        static HANDLE hInitEvent = CreateEvent(nullptr, true, false, nullptr);
        if (hInitEvent == nullptr)
            __debugbreak();

        static MyMutexManager *instance = nullptr;
        static long Once = false;
        // 단 한번만

        if (InterlockedCompareExchange(&Once, true, false) == false)
        {
            instance = new MyMutexManager();
            if (instance == nullptr)
                __debugbreak();
            SetEvent(hInitEvent);
        }
        else
        {
            WaitForSingleObject(hInitEvent, INFINITE);
        }

        return instance;
    }
    void RegisterTlsInfoAndHandle(stTlsLockInfo *info)
    {
        std::lock_guard<std::mutex> m_lock(m);
        LockInfos.insert(info);
    }
    void LogTlsInfo(const wchar_t *filename = L"TlsLockInfo.txt");

    std::unordered_set<stTlsLockInfo *> LockInfos;
    std::mutex m;
};
struct stTlsLockInfo
{
    stTlsLockInfo()
        : _size(0), waitLock(nullptr)
    {
        holding.reserve(30); // 30개의 Lock을 잡을일은 없겠지

        HANDLE hThread2 = GetCurrentThread();
        DuplicateHandle(GetCurrentProcess(), hThread2, GetCurrentProcess(), &hThread, 0, false, DUPLICATE_SAME_ACCESS);

    }

    void WriteLog(const wchar_t *filename)
    {
        const wchar_t ThreadStartFormat[] =
            L"┌──────────────────────────────────────────────────────────────┐\n"
            L"│ Thread : %-52s │\n"
            L"├──────────────────────────────────────────────────────────────┤\n";
        const wchar_t ThreadStartIDFormat[] =
            L"┌──────────────────────────────────────────────────────────────┐\n"
            L"│ Thread ID : %-48lu │\n"
            L"├──────────────────────────────────────────────────────────────┤\n";
        const wchar_t WaitStartFormat[] =
            L"│ WAIT  : %016p                                           │\n";

        const wchar_t HoldStartFormat[] =
            L"│ HOLD  : %016p                                           │\n";

        const wchar_t ThreadCloseFormat[] =
            L"└──────────────────────────────────────────────────────────────┘\n\n";


        wchar_t buffer[200];

        FILE *file;

        _wfopen_s(&file, filename, L"a+, ccs=UTF-16LE");
        if (file == nullptr)
            __debugbreak();
        wchar_t *threadName;

        GetThreadDescription(hThread, &threadName);
        if (wcslen(threadName) == 0)
        {
            StringCchPrintfW(buffer, _countof(buffer), ThreadStartIDFormat, hThread);
            fwrite(buffer, sizeof(wchar_t), wcslen(buffer), file);
        }
        else
        {
            StringCchPrintfW(buffer, _countof(buffer), ThreadStartFormat, threadName);
            fwrite(buffer, sizeof(wchar_t), wcslen(buffer), file);
        }

        if (waitLock != nullptr)
        {
            StringCchPrintfW(buffer, _countof(buffer), WaitStartFormat, waitLock);
            fwrite(buffer, sizeof(wchar_t), wcslen(buffer), file);
        }
        for (int i = 0; i < _size; i++)
        {
            StringCchPrintfW(buffer, _countof(buffer), HoldStartFormat, holding[i]);
            fwrite(buffer, sizeof(wchar_t), wcslen(buffer) , file);
        }
        fwrite(ThreadCloseFormat, sizeof(wchar_t), wcslen(ThreadCloseFormat), file);
        fclose(file);
    }
    // tls를 사용하여 동기화없이 접근하기.
    void *waitLock;
    std::vector<void *> holding; // bool 이 false 면 Shared , 1이면 Exclusive
    int _size;                   // holding의 길이.
    HANDLE hThread;
};

struct stMyMutex
{
    std::mutex m;
    _Acquires_lock_(m) void lock();
    _Releases_lock_(m) void unlock();
};

#ifdef MY_MUTEX
#define mutex stMyMutex
#endif
