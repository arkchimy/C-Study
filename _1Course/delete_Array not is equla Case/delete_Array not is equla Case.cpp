
#include <iostream>

class A
{
  public:
    ~A()
    {
        //num = 0;

    }
    int num;
};
int main()
{
    A *a = new A;
    delete[] a;

}
