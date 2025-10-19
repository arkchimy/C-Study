#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include "../../../_3Course/lib/CLockFreeQueue_lib/CLockFreeQueue_lib.h"
#include "../../../_3Course/lib/CLockFreeStack_lib/CLockFreeStack.h"
#include "../../../_3Course/lib/CSystemLog_lib/CSystemLog_lib.h"


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
       
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"GetFullPool Called emptyStack is nullptr  ,"
                                                                                       L"retval  :  %p , %lld ",
                                           retval, retval->m_size);
            return retval;
        }
        emptyPools.Push(emptyStack);
 
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"emptyPools.Push %p  , emptyPools.Size %lld",
                                       emptyStack, emptyPools.m_size);
        if (fullPools.m_size == 0)
        {
            retval = new ObjectPoolType;
            retval->Initalize(INIT_CAPACITY);

            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"GetFullPool Called emptyStack is %p  m_size %lld  retval  :  %p , %lld ",
                                           emptyStack, emptyStack->m_size,retval, retval->m_size);
            return retval;
        }
        retval = fullPools.Pop();
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"fullPools.Pop %p  , fullPools.Size %lld",
                                       retval, fullPools.m_size);
        return retval;
    }
    ObjectPoolType *GetEmptyPool(ObjectPoolType *fullStack)
    {
        ObjectPoolType *retval;
        if (fullStack == nullptr)
        {
            retval = new ObjectPoolType();
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"GetEmptyPool Called fullStack is nullptr  ,"
                                                                                       L"retval  :  %p , %lld ",
                                           retval, retval->m_size);
            return retval;
        }

        fullPools.Push(fullStack);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"fullPools.Push %p  , fullPools.Size %lld",
                                       fullStack, fullPools.m_size);

        if (emptyPools.m_size == 0)
        {
            retval = new ObjectPoolType();
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"GetEmptyPool Called emptyPools size : 0  ,"
                                                                                        L"new Create retval  :  %p , %lld ",
                                           retval, retval->m_size);
        }
        else
            retval = emptyPools.Pop();
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"GetEmptyPool Called emptyPools Size : %lld  retval  :  %p , %lld ",
                                       emptyPools.m_size,retval, retval->m_size);
        return retval;
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

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"New GetFullPool Called "
                                                                                   L"Current allocPool : %p Current allocPool size : %lld",
                                       allocPool, allocPool->m_size);

        releasePool = instance.GetEmptyPool(nullptr);
        assert(releasePool != nullptr);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"New GetEmptyPool Called "
                                                                                   L"Current releasePool : %p Current releasePool size : %lld",
                                       releasePool, releasePool->m_size);
    }
    ~stTlsObjectPool()
    {
        if (allocPool->m_size != 0)
        {

            instance.fullPools.Push(allocPool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"fullPools.Push Called "
                                                                                       L"allocPool : %p allocPool size : %lld",
                                           allocPool, allocPool->m_size);
        }
        else
        {
            instance.emptyPools.Push(allocPool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"emptyPools.Push Called "
                                                                                       L"allocPool : %p allocPool size : %lld",
                                           allocPool, allocPool->m_size);
        }
        if (releasePool->m_size != INIT_CAPACITY)
        {
            instance.emptyPools.Push(releasePool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"emptyPools.Push Called "
                                                                                       L"releasePool : %p releasePool size : %lld",
                                           releasePool, releasePool->m_size);
        }
        else
        {
            instance.fullPools.Push(releasePool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"fullPools.Push Called "
                                                                                       L"releasePool : %p releasePool size : %lld",
                                           releasePool, releasePool->m_size);
        }

    }
    PVOID Alloc()
    {
        if (allocPool->m_size == 0)
        {
            ObjectPoolType *swap = allocPool;
            allocPool = instance.GetFullPool(swap);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"GetFullPool Called "
                                                                                       L"before allocPool : %p\t Current allocPool : %p Current allocPool size : %lld",
                                           swap, allocPool, allocPool->m_size);
        }
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"Alloc Called  allocPool : size %lld",
                                       allocPool->m_size);
        return allocPool->Alloc();
    }
    void Release(PVOID node)
    {
        releasePool->Release(node);

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"Release Called "
                                                                                   L"Release : size %lld",
                                       releasePool->m_size);

        if (releasePool->m_size == INIT_CAPACITY)
        {
            ObjectPoolType *swap = releasePool;
            releasePool = instance.GetEmptyPool(swap);

            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"GetEmptyPool Called "
                                                                                       L"before releasePool : %p\t Current releasePool : %p Current releasePool size : %lld",
                                           swap, releasePool, releasePool->m_size);
        }
    }

    inline static stTlsObjectPoolManager<T> instance;

    ObjectPoolType *allocPool = nullptr;
    ObjectPoolType *releasePool = nullptr;
};
