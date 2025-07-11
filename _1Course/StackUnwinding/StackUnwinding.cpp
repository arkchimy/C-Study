#include <iostream>

struct LocalObj
{
    LocalObj() { std::cout << "생성자\n"; }
    ~LocalObj() { std::cout << "소멸자\n"; }
};
void Foo2() noexcept;
void Foo3() noexcept;

void foo() noexcept
{
    LocalObj obj;
    Foo3();
}
void Foo2() noexcept
{
    foo();
    //throw 1;
}
void Foo3() noexcept
{
    throw 1;
}
int main()
{
    foo();
    Foo2();
  /*  try
    {
        foo();
    }
    catch (...)
    {
        std::cout << "예외 포착\n";
    }*/
}
