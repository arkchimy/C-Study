#pragma once
#include <concepts>
#include <exception>
#include <type_traits>
class CLargePage
{
  public:
    CLargePage();
    char *Alloc(unsigned long long size , bool bLargePage);
    static CLargePage& GetInstance();
    inline static char *virtualMemoryBegin = nullptr;
};
