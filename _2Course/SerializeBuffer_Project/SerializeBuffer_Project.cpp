#include "CMessage.h"
#include <iostream>

CMessage msg;

int main()
{
    int a[100];
    for (int i = 0; i < 100; i++)
    {
        a[i] = i;
        msg << a[i];
    }
    int temp;
    for (int i = 0; i < 100; i++)
    {
        msg >> temp;
        std::cout << temp << "\n";
    }
}
