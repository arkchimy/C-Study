#pragma once
#include <Windows.h>
#include <iostream>
enum class en_LOG_LEVEL
{
    SYSTEM_Mode = 0,
    ERROR_Mode,
    DEBUG_Mode,

};
class CSystemLog
{
  public:
    static CSystemLog* GetInstance(); // Initalization 되어있는 SRWLOCK 객체를 넘겨 줄것.

    BOOL SetDirectory(const wchar_t *directoryFileRoute);

    void Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...);
    void LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen);

  private:
    CSystemLog() = default;
    BOOL CreateLogFile(const wchar_t *filename, SYSTEMTIME &stNowTime);

  public:
    FILE *LogFile = nullptr;
    const wchar_t *m_LogFileName;
    en_LOG_LEVEL m_Log_Level = en_LOG_LEVEL::SYSTEM_Mode;

    LONG64 m_seqNumber = 0;
};
