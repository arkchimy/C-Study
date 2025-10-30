#pragma once

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.


#include "../../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../../../_3Course/lib/CLockFreeQueue_lib/CLockFreeQueue_lib.h"
#include "../../../_3Course/lib/CLockFreeStack_lib/CLockFreeStack.h"
#include "../../../_3Course/lib/CSystemLog_lib/CSystemLog_lib.h"

#include "CObjectPool_UnSafeMT.h"


#define RT_ASSERT(x) \
    if (!(x))        \
        __debugbreak();

#define assert RT_ASSERT

extern int tlsPool_init_Capacity;

template <typename T>
using ObjectPoolType = CObjectPool_UnSafeMT<T>;

template <typename T>
struct stTlsObjectPoolManager
{
    using ManagerPool = CLockFreeStack<ObjectPoolType<T> *>;

    stTlsObjectPoolManager()
    {
        Parser parser;
        if (parser.LoadFile(L"Config.txt") == false)
            __debugbreak();

        if (parser.GetValue(L"tlsPool_init_Capacity", tlsPool_init_Capacity) == false)
            __debugbreak();
    }
    struct stNode
    {
        T data;
        stNode *next;
    };

    ObjectPoolType<T> *GetFullPool(ObjectPoolType<T> *emptyStack)
    {
        ObjectPoolType<T> *retval;

        emptyPools.Push(emptyStack);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s emptyPools.m_size :%lld \t InputData  : %p  Data Size : %lld",
                                       L"[ emptyPools.Push ]", emptyPools.m_size, emptyPools, emptyStack->m_size);
        if (fullPools.Pop(retval) == false)
        {
            if (emptyPools.Pop(retval))
            {
                retval->Initalize(tlsPool_init_Capacity);
                CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                               L"[ new tlsPool_init_Capacity => FullPool ]", retval);
            }
            else
            {
                retval = new ObjectPoolType<T>();
                retval->Initalize(tlsPool_init_Capacity);

                CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                               L"[ Create New FullPool ]", retval);
                CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s :  %p  => %p ",
                                               L"[ fullPool Change ] ", emptyStack, retval);
            }
            return retval;
        }

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s Size :%lld \t OutData  : %p  Data Size : %lld",
                                       L"fullPools.m_size", fullPools.m_size, retval, retval->m_size);

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s :  %p  => %p ",
                                       L"[ fullPool Change ]", emptyStack, retval);

        return retval;
    }
    ObjectPoolType<T> *GetEmptyPool(ObjectPoolType<T> *fullStack)
    {
        ObjectPoolType<T> *retval;

        fullPools.Push(fullStack);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s fullPools.m_size :%lld \t InputData  : %p  Data Size : %lld",
                                       L"[ fullPools.Push ]", fullPools.m_size, fullStack, fullStack->m_size);

        if (emptyPools.Pop(retval) == false)
        {
            retval = new ObjectPoolType<T>();
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - emptyPools.m_size == 0 ",
                                           L"[ Create New EmptyPool ]", retval);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  :  %p  => %p ",
                                           L"[ emptyPool Change ]", fullStack, retval);
            return retval;
        }

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  Size :%lld \t OutData  : %p  Data Size : %lld",
                                       L"[ emptyPools Pop ]", emptyPools.m_size, retval, retval->m_size);

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  :  %p  => %p ",
                                       L"[ emptyPool Change ]", fullStack, retval);

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
    struct stNode
    {
        T data;
        stNode *next;
    };

    stTlsObjectPool()
    {
        allocPool = new ObjectPoolType<T>();
        allocPool->Initalize(tlsPool_init_Capacity);
        assert(allocPool != nullptr);

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s  : %p -  TLSAlloc",
                                       L"[ Create New FullPool ]", allocPool);

        releasePool = new ObjectPoolType<T>();
        releasePool->Initalize(0);

        assert(releasePool != nullptr);

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s  : %p -  TLSAlloc",
                                       L"[ Create New EmptyPool ]", releasePool);
    }
    ~stTlsObjectPool()
    {
        if (allocPool->m_size != 0)
        {

            instance.fullPools.Push(allocPool);

            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  Size :%lld \t InputData  : %p  Data Size : %lld",
                                           L"[ fullPools Push ]", instance.fullPools.m_size, allocPool, allocPool->m_size);
        }
        else
        {
            instance.emptyPools.Push(allocPool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  Size :%lld \t InputData  : %p  Data Size : %lld",
                                           L"[ emptyPools Push ]", instance.emptyPools.m_size, allocPool, allocPool->m_size);
        }
        if (releasePool->m_size != tlsPool_init_Capacity)
        {
            instance.emptyPools.Push(releasePool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  Size :%lld \t InputData  : %p  Data Size : %lld",
                                           L"[ emptyPools Push ]", instance.emptyPools.m_size, releasePool, releasePool->m_size);
        }
        else
        {
            instance.fullPools.Push(releasePool);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  Size :%lld \t InputData  : %p  Data Size : %lld",
                                           L"[ fullPools Push ]", instance.fullPools.m_size, releasePool, releasePool->m_size);
        }
    }
    static PVOID Alloc()
    {


        stTlsObjectPool *pool = nullptr;
        ObjectPoolType<T> *swap;
        if (s_tlsIdx == TLS_OUT_OF_INDEXES)
        {

            // TODO :  TLS 가 부족한 경우의 대처는 프로그램 종료
            return nullptr;
        }

        pool = reinterpret_cast<stTlsObjectPool *>(TlsGetValue(s_tlsIdx));

        if (pool == nullptr)
        {
            pool = new stTlsObjectPool();
            if (pool == nullptr)
            {
                return nullptr;
            }
            TlsSetValue(s_tlsIdx, pool);
        }

        if (pool->allocPool->m_size == 0)
        {
            swap = pool->allocPool;
            pool->allocPool = instance.GetFullPool(swap);
            return pool->allocPool->Alloc();

            /*swap = pool->allocPool;
            if (pool->releasePool->m_size == 0)
            {
                pool->allocPool = instance.GetFullPool(swap);
                return pool->allocPool->Alloc();
            }
            pool->allocPool = pool->releasePool;
            pool->releasePool = swap;*/
        }
        PVOID node = pool->allocPool->Alloc();
        
      
        return node;
    }
    static void Release(PVOID node)
    {

        stTlsObjectPool *pool = nullptr;
        ObjectPoolType<T> *swap;
        if (s_tlsIdx == TLS_OUT_OF_INDEXES)
        {

            return;
        }

        pool = reinterpret_cast<stTlsObjectPool *>(TlsGetValue(s_tlsIdx));
        if (pool == nullptr)
        {
            pool = new stTlsObjectPool();
            if (pool == nullptr)
            {

                return;
            }
            TlsSetValue(s_tlsIdx, pool);
        }

        pool->releasePool->Release(node);

        swap = pool->releasePool;
        if (pool->releasePool->m_size == tlsPool_init_Capacity / 20)
        {

            pool->releasePool = instance.GetEmptyPool(swap);

            return;
        }

        /*
           //메모리가 계속 증가할것이다.
        if (pool->releasePool->m_size == tlsPool_init_Capacity)
           {
               swap = pool->releasePool;
               if (pool->allocPool->m_size == tlsPool_init_Capacity)
               {
                   pool->releasePool = instance.GetEmptyPool(swap);
                   return;
               }
               pool->releasePool = pool->allocPool;
               pool->allocPool = swap;
           }*/
    }

    inline static stTlsObjectPoolManager<T> instance;
    inline static DWORD s_tlsIdx = TlsAlloc();

    ObjectPoolType<T> *allocPool = nullptr;
    ObjectPoolType<T> *releasePool = nullptr;
};
