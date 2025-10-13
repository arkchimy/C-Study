#pragma once
#include <iostream>

enum class LogMode{
    DEBUG,
    ERROR,

};
class CSystemLog
{
  private:
    CSystemLog()
    {
    }
    void OpenFile(const wchar_t *filename);
    CSystemLog& GetInstance()
    {
        static CSystemLog instance;
        return instance;   
    }
  public:
    FILE *LogFile = nullptr;
    
};
