// Add_Func.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

int _cdecl Add(int a, int b)
{
    int c, d;
    c = a;
    c *= 2;
    d = c;
    return d;
}
int main()
{
    int retval;

    retval = Add(10, 20);
    std::cout << retval;
}

/*
* push 20
* push 10
* call Add()
* add ebp , 8
* 
* 
* push ebp
* mov ebp , esp
* sub esp , 8
* mov eax , dword ptr [ebp + 8]
* mov dword ptr[ebp - 4] , eax

* mov eax , dword ptr [ebp - 4]
* mul  eax , 2
* mov dword ptr[ebp - 8] , eax
* mov dword ptr[ebp - 4] , eax
* 
* mov esp , ebp
* pop ebp
* return eax
*/