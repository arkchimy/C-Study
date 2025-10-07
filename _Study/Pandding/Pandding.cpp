
#include <iostream>

struct stA
{
    int a; //  0 + 4
    alignas(64) char b; // 4 + (60) + 1 = 65;
    alignas(32) int c;  // 65 + 31 + 4 = 100
}; //  100 + (28)  = 128
int main()
{
    std::cout << sizeof(stA);

}

