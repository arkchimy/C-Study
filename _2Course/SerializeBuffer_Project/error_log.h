#pragma once
#define ERROR_FILE_LOG(x)                      \
    {                                          \
        FILE *file;                            \
        fopen_s(&file, "LastError.txt", "a+"); \
        __assume(file != nullptr);             \
        fwrite(x, 1, strlen(x), file);         \
        fclose(file);                          \
    }