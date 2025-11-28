// std_Thread_사용연습.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <thread>

#include <Windows.h>


class Animal
{
  public:
    virtual void Sound() {
        std::cout << "응애";
        
    }
};
void F1(void* arg)
{
    Animal *target = reinterpret_cast<Animal *>(arg);

    std::cout << "응애";
}
Animal *animal;
int main()
{
    animal = new Animal;
    std::thread a (F1,animal);
    std::thread b (std::move(a));
    SetThreadDescription(a.native_handle(), L"Std:Thread");
    a.join();
    animal->Sound();

    WaitForSingleObject(a.native_handle(), INFINITE);

}
