// 우선순위 연습.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <vector>

std::vector<class A*> vec;

class A
{
  public:
    int number;
    int aa;
};
int main()
{
    
    vec.push_back(new A());

    std::cout << "Hello World!\n";
}
