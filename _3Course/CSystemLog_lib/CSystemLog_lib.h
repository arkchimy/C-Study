#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include <Windows.h>
#include <iostream>
enum class en_LOG_LEVEL : DWORD
{
    SYSTEM_Mode = 0,
    ERROR_Mode,
    DEBUG_Mode,
    MAX,
};
class CSystemLog
{
  public:
    static CSystemLog *GetInstance(); // Initalization 되어있는 SRWLOCK 객체를 넘겨 줄것.

    BOOL SetDirectory(const wchar_t *directoryFileRoute);
    void SetLogLevel(const en_LOG_LEVEL &Log_Level);

    void Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...);
    void LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen);

  private:
    CSystemLog();
    BOOL GetLogFileName(const wchar_t *const filename, size_t strlen, SYSTEMTIME &stNowTime, __out wchar_t *const out);

  public:
    SRWLOCK srw_lock;

    LONG64 m_seqNumber = 0;

    en_LOG_LEVEL m_Log_Level = en_LOG_LEVEL::ERROR_Mode;
};
