#include <iostream>

class A
{
  public:
    virtual void T1()
    {
        printf("class A : this pointer %08p\n", this);
    }
};

class B
{
  public:
    virtual void T1()
    {
        printf("class B : this pointer %08p\n", this);
    }
};
class C : public A, public B
{
  public:
    virtual void T1()
    {
        printf("class C : this pointer %08p\n", this);
    }
};
int main()
{
    C *c = new C();
    c->T1();
    c->A::T1();
    c->B::T1();

}