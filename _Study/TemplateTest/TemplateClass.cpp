#include "TemplateClass.h"

template <>
void TemplateClass<int>::foo()
{
    std::cout << "int\n";
}
