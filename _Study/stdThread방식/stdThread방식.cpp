// stdThread방식.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <Windows.h>
#include <thread>


class A
{
  public:
    void WorkerThread() {
        std::cout << "WorkerThread\n";
    }
};
template <typename T>
using PMF = void (T::*)();

template <typename T>
unsigned ThreadTest(void *arg) 
{
    std::pair<T *, PMF<T>> *data = static_cast<std::pair<T *, PMF<T>> *>(arg);
    
    PMF<T> pmf = data->second;

    (data->first->*pmf)();

    delete data;

    return 0;
}

template <typename T>
HANDLE MyCreateThread(void(T::*pmf)(),T* instance) 
{
    auto arg = new std::pair<T *, PMF<T>>(instance, pmf);
    
    return (HANDLE)_beginthreadex(nullptr, 0, &ThreadTest<T>, (void*)(arg), 0, nullptr);

}
int main()
{
    A a;
    HANDLE hWorkerThread = MyCreateThread<A>(&A::WorkerThread, &a);
    std::thread(&A::WorkerThread, &a);

    WaitForSingleObject(hWorkerThread, INFINITE);
    CloseHandle(hWorkerThread);
}
