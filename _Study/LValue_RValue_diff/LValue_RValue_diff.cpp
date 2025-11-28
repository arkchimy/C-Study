// LValue_RValue_diff.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

struct stNode
{
    stNode(int a) : _a(a) {}
    void foo() { std::cout << "dsd\n"; }
    int _a;
};
int main()
{
    stNode node = stNode(1);
}

