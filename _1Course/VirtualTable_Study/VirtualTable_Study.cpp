
#include <iostream>

class A
{
  public:
    virtual ~A()
    {
        A *a = this;
        a->T1();
    }
    virtual void T1() = 0;
    int num = 0xffffff;
};
class B : public A
{
  public:
    virtual void T1() { num = 10; }
};
class C : public B
{
  public:
    virtual void T1() { num = 10000; }
};
int main()
{

    A *c = new C();


    delete c;
    std::cout << "Hello World!\n";
}
