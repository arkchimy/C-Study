#pragma once
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
    static CSystemLog* GetInstance(); // Initalization �Ǿ��ִ� SRWLOCK ��ü�� �Ѱ� �ٰ�.

    BOOL SetDirectory(const wchar_t *directoryFileRoute);
    void SetLogLevel(const en_LOG_LEVEL &Log_Level);

    void Log(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szStringFormat, ...);
    void LogHex(WCHAR *szType, en_LOG_LEVEL LogLevel, WCHAR *szLog, BYTE *pByte, int iByteLen);

  private:
    CSystemLog() ;
    BOOL GetLogFileName(const wchar_t *const filename, size_t strlen, SYSTEMTIME &stNowTime, __out wchar_t *const out);

  public:
    SRWLOCK srw_lock;

    LONG64 m_seqNumber = 0;

    en_LOG_LEVEL m_Log_Level = en_LOG_LEVEL::ERROR_Mode;


};
