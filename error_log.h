#pragma once
#include <iostream>
#include <strsafe.h>

#define ERROR_BUFFER_SIZE 100
//str�� const wchar_t* ���ڿ� ��� �� ��!
#define ERROR_FILE_LOG(LogFilename, str)                                               \
    {                                                                                  \
        FILE *file;                                                                    \
        DWORD lastError = GetLastError();                                              \
        const WCHAR *format = L"str : %s \t GetLastError : %d\n";                      \
        WCHAR errorBuffer[ERROR_BUFFER_SIZE];                                          \
        StringCchPrintfW(errorBuffer, ERROR_BUFFER_SIZE, format, str, GetLastError()); \
        fopen_s(&file, LogFilename, "a+");                                             \
        if ((file != nullptr))                                                         \
        {                                                                              \
            fwrite(errorBuffer, 1, wcslen(errorBuffer), file);                         \
            fclose(file);                                                              \
        }                                                                              \
    }
