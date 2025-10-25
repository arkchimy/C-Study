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

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s Size :%lld \t InputData  : %p  Data Size : %lld",
                                       L"[ emptyPools Push ]", emptyPools.m_size, emptyStack, emptyStack->m_size);

        if (fullPools.m_size == 0)
        {
            retval = new ObjectPoolType<T>();
            retval->Initalize(tlsPool_init_Capacity);

            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                           L"[ Create New FullPool ]", retval);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s :  %p  => %p ",
                                           L"[ fullPool Change ] ", emptyStack, retval);
            return retval;
        }
        else
            fullPools.Pop(retval);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s Size :%lld \t OutData  : %p  Data Size : %lld",
                                       L"[ fullPools Pop ]", fullPools.m_size, retval, retval->m_size);

        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s :  %p  => %p ",
                                       L"[ fullPool Change ]", emptyStack, retval);

        return retval;
    }
    ObjectPoolType<T> *GetEmptyPool(ObjectPoolType<T> *fullStack)
    {
        ObjectPoolType<T> *retval;

        fullPools.Push(fullStack);
        CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s Size :%lld \t InputData  : %p  Data Size : %lld",
                                       L"[fullPools Push]", fullPools.m_size, fullStack, fullStack->m_size);

        if (emptyPools.m_size == 0)
        {
            retval = new ObjectPoolType<T>();
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - emptyPools.m_size == 0 ",
                                           L"[ Create New EmptyPool ]", retval);
            CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode, L"%15s  :  %p  => %p ",
                                           L"[ emptyPool Change ]", fullStack, retval);
            return retval;
        }
        else
            emptyPools.Pop(retval);
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
        if (s_tlsIdx == TLS_OUT_OF_INDEXES)
        {
            CSystemLog::GetInstance()->Log(L"TLS", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s ",
                                           L"stTlsObjectPool", L"Alloc ReturnValue : TLS_OUT_OF_INDEXES ");

            // TODO :  TLS 가 부족한 경우의 대처는 프로그램 종료
            return nullptr;
        }

        pool = reinterpret_cast<stTlsObjectPool *>(TlsGetValue(s_tlsIdx));

        if (pool == nullptr)
        {
            pool = new stTlsObjectPool();
            if (pool == nullptr)
            {
                CSystemLog::GetInstance()->Log(L"TLS", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s %15s %05d",
                                               L"stTlsObjectPool", L"new Return Nullptr ", L"GetLastError : ", GetLastError());
                return nullptr;
            }
            TlsSetValue(s_tlsIdx, pool);
        }

        if (pool->allocPool->m_size == 0)
        {
            ObjectPoolType<T> *swap = pool->allocPool;
            pool->allocPool = instance.GetFullPool(swap);
        }

        return pool->allocPool->Alloc();
    }
    static void Release(PVOID node)
    {
        stTlsObjectPool *pool = nullptr;

        if (s_tlsIdx == TLS_OUT_OF_INDEXES)
        {
            CSystemLog::GetInstance()->Log(L"TLS", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s ",
                                           L"stTlsObjectPool", L"Release ReturnValue : TLS_OUT_OF_INDEXES ");

            // TODO :  TLS 가 부족한 경우의 대처는 프로그램 종료
            return;
        }

        pool = reinterpret_cast<stTlsObjectPool *>(TlsGetValue(s_tlsIdx));
        if (pool == nullptr)
        {
            pool = new stTlsObjectPool();
            if (pool == nullptr)
            {
                CSystemLog::GetInstance()->Log(L"TLS", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s %15s %05d",
                                               L"stTlsObjectPool", L"new Return Nullptr ", L"GetLastError : ", GetLastError());
                // TODO : 만일 실패하면 Manager 객체에서 바로 가져오는 방안 생각하기.
                return;
            }
            TlsSetValue(s_tlsIdx, pool);
        }

        pool->releasePool->Release(node);

        if (pool->releasePool->m_size == tlsPool_init_Capacity)
        {
            ObjectPoolType<T> *swap = pool->releasePool;
            pool->releasePool = instance.GetEmptyPool(swap);
        }
    }

    inline static stTlsObjectPoolManager<T> instance;
    inline static DWORD s_tlsIdx = TlsAlloc();

    ObjectPoolType<T> *allocPool = nullptr;
    ObjectPoolType<T> *releasePool = nullptr;
};
