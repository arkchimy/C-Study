#include <iostream>
#include <Windows.h>
#include <process.h>

class CLanServer
{
  public:
    void WorkerThread()
    {
        std::cout << "WorkerThread";
    }; // 해당 함수를 시작하는 Thread를 생성하기를 원한다면.
};
// 과정
// WorkerThread의 함수포인터와 Instance를 매개변수로 전달해줘야 한다.
// std::thread도 __beginThread에 넘겨줄 기본 함수가 존재한다.
// 그 기본 함수는 매개변수로 받고, 매개변수로 받은 함수를 해당 instance 를 이용해 멤버함수를 호출한다.

template <typename T>
HANDLE MyCreateThread(void (T::*pmf)(void), T *instance = this) // 이거 되나?
{
    std::pair<void (T::*)(void), T *> arg({pmf, instance});
    return (HANDLE)_beginthreadex(nullptr, 0, &StartRoutine<T>, arg, 0, nullptr);
}

template <typename T>
unsigned StartRoutine(void* arg)
{
    std::pair<void (T::*)(void), T *> data = static_cast<std::pair<void (T::*)(void), T *>>(arg);
    T *instance = data->second;
    (instance->*)data->first();
}

int main()
{

}