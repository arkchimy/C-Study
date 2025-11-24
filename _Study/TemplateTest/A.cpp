
#include "A.h"

extern template void TemplateClass<int>::foo();

void A::Fuct_A()
{
    TemplateClass<int> instacne1;
    instacne1.foo();

}
