#include <iostream>
#include "../../_3Course/lib/CLockFreeQueue_lib/CLockFreeQueue_lib.h"
#include "../../_3Course/lib/CLockFreeStack_lib/CLockFreeStack.h"

#define INITALIZE_POOL_SIZE 500
template <typename T>
struct stTlsObjectPoolManager
{
    CLockFreeStack<T>* GetPool(const CLockFreeStack<T>* emptyStack) 
    {
        CLockFreeStack<T> *retval;

        emptyPool.Push(emptyStack);
        if (fullPool.m_size == 0)
        {
            retval = new CLockFreeStack<T>();
            for (int i = 0; i < INITALIZE_POOL_SIZE; i++)
                retval->Push(T());
            return retval;
        }
        return fullPool.Pop();
    }
    CLockFreeStack<CLockFreeStack<T> *> fullPool;
    CLockFreeStack<CLockFreeStack<T> *> emptyPool;
};


/*
    * Pool을 TLS로 Thread마다 갖고있게 한다.
    * 특정 Q Instance를 사용하기 원한다면 만일 그 Q에 Pop , Push를 사용하게된다면, 
    * Node들이 Q마다 존재하는 Pool에 Pop과 Push를 진행한다.
    * 그러면  Pool에 대한 경합이 발생한다.
    * 이 경합을 줄이기 위해 이 Pool을 TLS Pool에 넣도록 구현하자.
*/

template <typename T>
struct stTlsObjectPool
{
    T* Alloc() 
    {
        if (allocPool->m_size == 0)
        {
            allocPool = instance.GetPool(allocPool);
        }
        return reinterpret_cast<T*>(&allocPool->Pop());
    }
    void Release(T *) 
    {
    }

    inline static stTlsObjectPoolManager<T> instance;
    CLockFreeStack<T> *allocPool;
    CLockFreeStack<T> *releasePool;
};


int main()
{
    std::cout << "Hello World!\n";
}
