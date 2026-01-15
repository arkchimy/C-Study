// R_valueTest.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>

void F1(int &&a)
{
    std::cout << "F1(int&&a);\n";
}
void F1(int &a)
{
    std::cout << "F1(int&a);\n";
}
#include <vector>
std::vector<int> vec;

int main()
{
    vec.resize(10, 0);
    auto &&num = std::move(vec[0]); // RValue 반환
    F1(num); // F1(int &a) 출력
    F1(std::move(num)); // F1(int &&a); 출력
    F1(std::forward<int>(num)); // F1(int &&a); 출력
}
