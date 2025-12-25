#include "stMyMutex.h"


extern thread_local stTlsLockInfo tls_LockInfo;

_Acquires_exclusive_lock_(m) void stMyMutex::lock()
{
    auto iter = tls_LockInfo.holding.begin();
    tls_LockInfo.waitLock = &m;

    for (iter; iter != tls_LockInfo.holding.end(); iter++)
    {
        if (*iter == &m)
            break;
    }
    if (iter != tls_LockInfo.holding.end())
    {
        MyMutexManager::GetInstance()->RequestCreateLogFile_And_Debugbreak();
    }

    m.lock();
    tls_LockInfo.waitLock = nullptr;
    tls_LockInfo.holding.push_back(&m);
    tls_LockInfo._size++;
    
}
_Releases_exclusive_lock_(m) void stMyMutex::unlock()
{
    auto iter = tls_LockInfo.holding.begin();
    for (iter; iter != tls_LockInfo.holding.end(); iter++)
    {
        if (*iter == &m)
            break;
    }
    if (iter == tls_LockInfo.holding.end())
        MyMutexManager::GetInstance()->RequestCreateLogFile_And_Debugbreak();

    tls_LockInfo.holding.erase(iter);
    tls_LockInfo._size--;
    m.unlock();
}

_Acquires_shared_lock_(m) void stMyMutex::lock_shared()
{
    tls_LockInfo.waitLock = &m;

    auto iter = tls_LockInfo.shared_holding.begin();
    for (iter; iter != tls_LockInfo.shared_holding.end(); iter++)
    {
        if (*iter == &m)
            break;
    }
    if (iter != tls_LockInfo.shared_holding.end())
        MyMutexManager::GetInstance()->RequestCreateLogFile_And_Debugbreak();

    m.lock_shared();
    tls_LockInfo.waitLock = nullptr;
    // LockShared에서 Holding을 표시해둘까?


    tls_LockInfo.shared_holding.push_back(&m);
    tls_LockInfo._shared_size++;
    
}
_Releases_shared_lock_(m) void stMyMutex::unlock_shared()
{
    auto iter = tls_LockInfo.shared_holding.begin();
    for (iter; iter != tls_LockInfo.shared_holding.end(); iter++)
    {
        if (*iter == &m)
            break;
    }
    if (iter == tls_LockInfo.shared_holding.end())
        MyMutexManager::GetInstance()->RequestCreateLogFile_And_Debugbreak();

    tls_LockInfo.shared_holding.erase(iter);
    tls_LockInfo._shared_size--;
    m.unlock();
}

void MyMutexManager::LogTlsInfo(const wchar_t *filename)
{
    // Log를 위한 Lock
    std::lock_guard<std::mutex> m_lock(Log_m);
    {
        std::lock_guard<std::shared_mutex> m_lock(m);
        for (auto &info : LockInfos)
        {
            SuspendThread(info->hThread);
        }
        FILE *file;

        _wfopen_s(&file, filename, L"w, ccs=UTF-16LE");
        if (file == nullptr)
            __debugbreak();

        fclose(file);

        for (auto &info : LockInfos)
        {
            info->WriteLog(filename);
        }

        for (auto &info : LockInfos)
        {
            ResumeThread(info->hThread);
        }
    }
}

void MyMutexManager::RequestCreateLogFile_And_Debugbreak()
{
    SetEvent(hMutexLogEvent);
    Sleep(INFINITE);
}
