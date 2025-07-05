// 2의 보수.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

int main()
{

    long temp = 0;
    volatile unsigned long number = ~temp;
    printf("~0 : %d \n", number);
    number = 0xddccbbaa;
    char ch = number;

    printf("ch = number; : %d\n", ch);
    number += ch;

    ch = 0xddccbbaa;
    printf("ch = 0xddccbbaa ; : %d\n", ch);

    number = ch;
    printf("number : %d \n", number);

    
    return 0;
}