#include <iostream>
#include <Windows.h>
#include <thread>

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
unsigned StartRoutine(void* arg)
{
    std::pair<void (T::*)(void), T *>* data = static_cast<std::pair<void (T::*)(void), T *>*>(arg);
    (data->second->*data->first)(); //  (괄호 주의)
    delete data;
    return 0;
}



struct MyThread
{

    template <typename T>
    MyThread(void (T::*pmf)(void), T* instance)
    {
        // 소멸자에서 delete 
        std::pair<void (T::*)(void), T *> *arg = new std::pair<void (T::*)(void), T *>({pmf,instance}); 
        _hThread = (HANDLE)_beginthreadex(nullptr, 0, &StartRoutine<T>, arg, 0, nullptr);
    }
    ~MyThread()
    {
        CloseHandle(_hThread);
    }
    HANDLE _hThread;
};

int main()
{
    CLanServer server;
    CLanServer* pServer = &server;
    MyThread th2(&CLanServer::WorkerThread, pServer);
    WaitForSingleObject(th2._hThread, INFINITE);
}