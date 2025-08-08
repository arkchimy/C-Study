#include "error_log.h"
#include "pch.h"

void HEX_FILE_LOG(wchar_t *LogFilename, void *begin, size_t size, size_t rowChar)
{
    FILE *file = nullptr;
    DWORD lastError = GetLastError();
    wchar_t buffer[100 * 3 + 1 + 1];
    DWORD current = 0;
    char *current = reinterpret_cast<char *>(begin);
    char *end = current + size;
    /*   for (; current != end; current++)


memcpy(buffer, begin + current, )*/

    const WCHAR *format = L"%02X  ";
    WCHAR errorBuffer[ERROR_BUFFER_SIZE];
    StringCchPrintfW(errorBuffer, ERROR_BUFFER_SIZE, format, val);
    _wfopen_s(&file, LogFilename, L"a+,ccs=UTF-16LE");
    if (file != nullptr)
    {
        __assume(file != nullptr);
        fwrite(errorBuffer, 2, wcslen(errorBuffer), file);
        fclose(file);
    }
}