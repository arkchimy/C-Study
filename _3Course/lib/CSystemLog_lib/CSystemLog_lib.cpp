// CSystemLog_lib.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include "CSystemLog_lib.h"
#include "../../../error_log.h"
#include <strsafe.h>

CSystemLog::CSystemLog()
{
    InitializeSRWLock(&srw_lock);
}

// 에러 발생 시 이중으로 Log를 작성하여 에러상황을 알려보는 시도.
BOOL CSystemLog::GetLogFileName(const wchar_t *const filename, size_t strlen, SYSTEMTIME &stNowTime, __out wchar_t *const out)
{

    HRESULT cch_retval;

    cch_retval = StringCchPrintfW(out, strlen, L"%d%d_%s.txt", stNowTime.wYear, stNowTime.wMonth, filename);
    if (cch_retval != S_OK)
    {
        ERROR_FILE_LOG(L"StringCchPrintf_Error.txt", L"StringCchPrintfW Error");
        __debugbreak();
        return false;
    }

    return true;
}

BOOL CSystemLog::SetDirectory(const wchar_t *directoryFileRoute)
{
    return SetCurrentDirectoryW(directoryFileRoute);
}

void CSystemLog::SetLogLevel(const en_LOG_LEVEL &Log_Level)
{
    m_Log_Level = Log_Level;
}

CSystemLog *CSystemLog::GetInstance()
{
    static CSystemLog instance;
    return &instance;
}

void CSystemLog::Log(const WCHAR *szType, en_LOG_LEVEL LogLevel, const WCHAR *szStringFormat, ...)
{
    // szType 별로 파일이 나옴.
    // szStringFormat 는 Log 시점에 남기고싶은 정보를 남기기 위함.
    // 고정적으로  [szType] [2015-09-11 19:00:00 / LogLevel / 0000seqNumber] 로그문자열.

    static WCHAR format[] = L"[ %-12s] [ %04d-%02d-%02d %02d:%02d:%03d  /%-8s/%08lld] \t[Thread ID : %06d] \t";
    static const WCHAR *Logformat[(DWORD)en_LOG_LEVEL::MAX] =
        {
            L"SYSTEM",
            L"ERROR",
            L"DEBUG",
            };

    WCHAR LogHeaderBuffer[FILENAME_MAX * 2];
    WCHAR LogPayLoadBuffer[FILENAME_MAX];
    WCHAR LogFileName[FILENAME_MAX];

    HRESULT cchRetval;

    va_list va;
    SYSTEMTIME stNowTime;
    LONG64 local_SeqNumber;

    FILE *LogFile;

    if (LogLevel > m_Log_Level)
        return;

    local_SeqNumber = _InterlockedIncrement64(&m_seqNumber);

    GetLocalTime(&stNowTime);

    if (GetLogFileName(szType, FILENAME_MAX, stNowTime, LogFileName) == false)
    {
        __debugbreak();
        return;
    }

    StringCchPrintfW(LogHeaderBuffer, sizeof(LogHeaderBuffer) / sizeof(wchar_t), format,
                     szType, stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
                     stNowTime.wHour, stNowTime.wMinute, stNowTime.wMilliseconds,
                     Logformat[(DWORD)LogLevel], local_SeqNumber,GetCurrentThreadId());

    //[Battle] [2015-09-11 19:00:00 / DEBUG / 000000001] 로그문자열.

    va_start(va, szStringFormat);
    cchRetval = StringCchVPrintfW(LogPayLoadBuffer, sizeof(LogPayLoadBuffer) / sizeof(wchar_t), szStringFormat, va);
    va_end(va);

    if (cchRetval != S_OK)
    {
        ERROR_FILE_LOG(L"StringCchVPrintfW.txt", L"StringCchVPrintfW Error");
    }

    StringCchCatW(LogHeaderBuffer, sizeof(LogHeaderBuffer) / sizeof(wchar_t), LogPayLoadBuffer);

    DWORD idx = wcslen(LogHeaderBuffer);
    LogHeaderBuffer[idx] = L'\n';
    LogHeaderBuffer[idx + 1] = 0;

    AcquireSRWLockExclusive(&srw_lock);
    _wfopen_s(&LogFile, LogFileName, L"a+ , ccs = UTF-16LE");

    if (LogFile == nullptr)
    {
        ERROR_FILE_LOG(L"FileOpen_Error.txt", L"_wfopen_s Error");
        ReleaseSRWLockExclusive(&srw_lock);
        return;
    }

    fwrite(LogHeaderBuffer, 2, wcslen(LogHeaderBuffer), LogFile);
    fclose(LogFile);
    ReleaseSRWLockExclusive(&srw_lock);
}

void CSystemLog::LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen)
{
}
