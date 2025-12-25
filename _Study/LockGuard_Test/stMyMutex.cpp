#include "stMyMutex.h"


extern thread_local stTlsLockInfo tls_LockInfo;

_Acquires_lock_(m) void stMyMutex::lock()
{
    tls_LockInfo.waitLock = &m;
    m.lock();
    tls_LockInfo.waitLock = nullptr;
    tls_LockInfo.holding.push_back(&m);
    tls_LockInfo._size++;
    
}
_Releases_lock_(m) void stMyMutex::unlock()
{
    auto iter = tls_LockInfo.holding.begin();
    for (iter; iter != tls_LockInfo.holding.end(); iter++)
    {
        if (*iter == &m)
            break;
    }
    if (iter == tls_LockInfo.holding.end())
        __debugbreak();

    tls_LockInfo.holding.erase(iter);
    tls_LockInfo._size--;
    m.unlock();
}

void MyMutexManager::LogTlsInfo(const wchar_t *filename)
{
    std::lock_guard<std::mutex> m_lock(m);
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