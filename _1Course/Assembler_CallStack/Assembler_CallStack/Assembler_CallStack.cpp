// Assembler_CallStack.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <Windows.h>
int Test(int a, int b)
{
    int x, y;
    x = 0;
    y = 0;

    a = a * b;
    x = a;

    return x;
}
int Ebpcorruntion() 
{
    int x = 0;
    int *pX = &x; 
    Sleep(0);
    
    pX += 3;
    *pX = x; //이전 ebp의 오염

    return 0;
}

int main()
{
    int x = 0;
   // printf("테스트하고싶다");
    Test(10, 20);
   Ebpcorruntion();
    x = 100;

    
}
