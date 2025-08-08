#pragma once
#include <iostream>
#include <strsafe.h>

#define ERROR_BUFFER_SIZE 100
// str은 const wchar_t* 문자열 상수 일 것!
#if 0
void ERROR_FILE_LOG(const wchar_t *LogFilename, const wchar_t *str);
#else
#define ERROR_FILE_LOG(LogFilename, str)                                          \
    do                                                                            \
    {                                                                             \
        FILE *file = nullptr;                                                     \
        DWORD lastError = GetLastError();                                         \
        const WCHAR *format = L"str : %s \t GetLastError : %d\n";                 \
        WCHAR errorBuffer[ERROR_BUFFER_SIZE];                                     \
        StringCchPrintfW(errorBuffer, ERROR_BUFFER_SIZE, format, str, lastError); \
        _wfopen_s(&file, LogFilename, L"a+,ccs=UTF-16LE");                        \
        if (file != nullptr)                                                      \
        {                                                                         \
            __assume(file != nullptr);                                            \
            fwrite(errorBuffer, 2, wcslen(errorBuffer), file);                    \
            fclose(file);                                                         \
        }                                                                         \
    } while (0)
#endif
void HEX_FILE_LOG(const wchar_t *LogFilename, void *begin, size_t size);               

