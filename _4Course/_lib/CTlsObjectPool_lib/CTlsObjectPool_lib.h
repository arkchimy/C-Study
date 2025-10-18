#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include "../../../_3Course/lib/CLockFreeQueue_lib/CLockFreeQueue_lib.h"
#include "../../../_3Course/lib/CLockFreeStack_lib/CLockFreeStack.h"


#define RT_ASSERT(x) \
    if (!(x))        \
        __debugbreak();

#define assert RT_ASSERT

#define INIT_CAPACITY 500

template <typename T>
struct stTlsObjectPoolManager
{

    struct stNode
    {
        T data;
        stNode *next;
    };

    using ObjectPoolType = CObjectPool<T>;
    using ManagerPool = CLockFreeStack<ObjectPoolType *>;

    ObjectPoolType *GetFullPool(ObjectPoolType *emptyStack)
    {
        ObjectPoolType *retval;
        if (emptyStack == nullptr)
        {
            retval = new ObjectPoolType;
            retval->Initalize(INIT_CAPACITY);

            return retval;
        }
        emptyPools.Push(emptyStack);
        if (fullPools.m_size == 0)
        {
            retval = new ObjectPoolType;
            retval->Initalize(INIT_CAPACITY);

            return retval;
        }
        return fullPools.Pop();
    }
    ObjectPoolType *GetEmptyPool(ObjectPoolType *fullStack)
    {
        ObjectPoolType *retval;
        if (fullStack == nullptr)
        {
            return new ObjectPoolType();
        }

        fullPools.Push(fullStack);
        if (emptyPools.m_size == 0)
        {
            return new ObjectPoolType();
        }
        return emptyPools.Pop();
    }

    ManagerPool fullPools;
    ManagerPool emptyPools;

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
    using ObjectPoolType = CObjectPool<T>;
    struct stNode
    {
        T data;
        stNode *next;
    };

    stTlsObjectPool()
    {
        allocPool = instance.GetFullPool(nullptr);
        assert(allocPool != nullptr);
        releasePool = instance.GetEmptyPool(nullptr);
        assert(releasePool != nullptr);
    }
    ~stTlsObjectPool()
    {
        instance.fullPools.Push(allocPool);
        instance.fullPools.Push(releasePool);

    }
    PVOID Alloc()
    {
        if (allocPool->m_size == 0)
        {
            allocPool = instance.GetFullPool(allocPool);
        }
        return allocPool->Alloc();
    }
    void Release(PVOID node)
    {
        releasePool->Release(node);
        if (releasePool->m_size == INIT_CAPACITY)
        {
            releasePool = instance.GetEmptyPool(releasePool);
        }
    }

    inline static stTlsObjectPoolManager<T> instance;

    ObjectPoolType *allocPool = nullptr;
    ObjectPoolType *releasePool = nullptr;
};
