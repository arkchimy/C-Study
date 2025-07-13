#include <iostream>

#include <thread>
#include <Windows.h>

//둘다 일드 프로세서가 있다.


CRITICAL_SECTION cs_test;
SRWLOCK srw_test;

unsigned int srwThread(void *arg) 
{
    AcquireSRWLockExclusive(&srw_test);
    ReleaseSRWLockExclusive(&srw_test);
    return 1;
}
unsigned int csThread(void *arg) 
{
    EnterCriticalSection(&cs_test);
    LeaveCriticalSection(&cs_test);
    return 0;
}
int main()
{
    InitializeCriticalSection(&cs_test);
    InitializeSRWLock(&srw_test);


    _beginthreadex(nullptr, 0, srwThread, nullptr, 0, nullptr);
    _beginthreadex(nullptr, 0, csThread, nullptr, 0, nullptr);

    Sleep(30);
    AcquireSRWLockExclusive(&srw_test);
    ReleaseSRWLockExclusive(&srw_test);
    
    EnterCriticalSection(&cs_test);
    LeaveCriticalSection(&cs_test);



}
