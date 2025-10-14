#include "CSystemLog.h"
#include <strsafe.h>

// 에러 발생 시 이중으로 Log를 작성하여 에러상황을 알려보는 시도.
BOOL CSystemLog::CreateLogFile(const wchar_t *filename, SYSTEMTIME &stNowTime)
{
    wchar_t LogFileName[FILENAME_MAX];
    HRESULT cch_retval;

    cch_retval = StringCchPrintfW(LogFileName, FILENAME_MAX, L"%d_%d_%s.txt", stNowTime.wYear, stNowTime.wMonth, filename);
    if (cch_retval != S_OK)
    {
        // ERROR_FILE_LOG(L"StringCchPrintf_Error.txt", L"StringCchPrintfW Error");
        return false;
    }
    m_LogFileName = LogFileName;

    _wfopen_s(&LogFile, m_LogFileName, L"a+");

    if (LogFile == nullptr)
    {
        // ERROR_FILE_LOG(L"FileOpen_Error.txt", L"_wfopen_s Error");
        return false;
    }
    fclose(LogFile);

    return true;
}

BOOL CSystemLog::SetDirectory(const wchar_t *directoryFileRoute)
{
    return SetCurrentDirectoryW(directoryFileRoute);
}

CSystemLog *CSystemLog::GetInstance()
{
    static CSystemLog instance;
    return &instance;
}

void CSystemLog::Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...)
{
    // szType 별로 파일이 나옴.
    // szStringFormat 는 Log 시점에 남기고싶은 정보를 남기기 위함.
    // 고정적으로  [szType] [2015-09-11 19:00:00 / LogLevel / 0000seqNumber] 로그문자열.

    static WCHAR format[] = L"[%s] [%d-%d-%d %d:%d:%d / %s / %08lld]";
    WCHAR LogBuffer[FILENAME_MAX];

    HRESULT cchRetval;

    va_list va;
    SYSTEMTIME stNowTime;
    LONG64 local_SeqNumber;
    if (LogLevel > m_Log_Level)
        return;

    local_SeqNumber = _InterlockedIncrement64(&m_seqNumber);

    GetLocalTime(&stNowTime);

    StringCchPrintfW(LogBuffer, FILENAME_MAX, format,
        szType, stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay,
                     stNowTime.wHour, stNowTime.wMinute, stNowTime.wMilliseconds,
                     LogLevel, local_SeqNumber);

    wprintf(L"%s\n", LogBuffer);

    StringCchCatW(LogBuffer, FILENAME_MAX, szStringFormat);
    // TODO : 육안으로 확인후 제거 코드.
    wprintf(L"%s\n", LogBuffer);


    //[Battle] [2015-09-11 19:00:00 / DEBUG / 000000001] 로그문자열.
    va_start(va, szStringFormat);
    if (CreateLogFile(szType, stNowTime) == false)
        __debugbreak();

    cchRetval = StringCchVPrintfW(LogBuffer, FILENAME_MAX, LogBuffer, va);
    // TODO : 육안으로 확인후 제거 코드.
    wprintf(L"%s\n", LogBuffer);
    __debugbreak();
    va_end(va);

    if (cchRetval != S_OK)
    {
        // ERROR_FILE_LOG(L"StringCchVPrintfW.txt", L"StringCchVPrintfW Error");
    }

    __assume(LogFile != nullptr);

    switch (LogLevel)
    {
    //[Battle] [2015-09-11 19:00:00 / DEBUG / 000000001] 로그문자열.
    case en_LOG_LEVEL::DEBUG_Mode:
    case en_LOG_LEVEL::ERROR_Mode:
    case en_LOG_LEVEL::SYSTEM_Mode:
        _wfopen_s(&LogFile, m_LogFileName, L"a+");
        fwrite(LogBuffer, 2, FILENAME_MAX / sizeof(wchar_t), LogFile);
        fclose(LogFile);
    }
}

void CSystemLog::LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen)
{
}
