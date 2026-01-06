#include "WinThread.h"
#include <iostream>

class A
{
  public:
    void WorkerThread() {
        std::cout << "WorkerThread\n";        
    }
};
void example()
{
    A a;
    WinThread thread(&A::WorkerThread,&a);

}




WinThread::~WinThread()
{
    CloseHandle(_hThread);
}
