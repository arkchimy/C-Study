﻿// CTlsObjectPool_lib.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//

#include "pch.h"
#include "CTlsObjectPool_lib.h"
#include  "../../../_3Course/lib/SerializeBuffer_exception/SerializeBuffer_exception.h"

int tlsPool_init_Capacity;


template <>
ObjectPoolType<CMessage> * stTlsObjectPoolManager<CMessage>::GetFullPool(ObjectPoolType<CMessage> *emptyStack)
{
    ObjectPoolType<CMessage> *retval;

    emptyPools.Push(emptyStack);

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s emptyPools.m_size :%lld \t InputData  : %p  Data Size : %lld",
                                    L"[ emptyPools.Push ]", emptyPools.m_size, emptyStack, emptyStack->m_size);

    if (fullPools.Pop(retval) == false)
    {
        if (emptyPools.Pop(retval))
        {
            retval->Initalize(tlsPool_init_Capacity);
            CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                            L"[ Change Empty => FullPool ]", retval);
        }
        else
        {
            retval = new ObjectPoolType<CMessage>();
            retval->Initalize(tlsPool_init_Capacity);

            CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                            L"[ Create New FullPool ]", retval);
            CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s :  %p  => %p ",
                                            L"[ fullPool Change ] ", emptyStack, retval);
        }
        return retval;
    }

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s Size :%lld \t OutData  : %p  Data Size : %lld",
                                    L"fullPools.m_size", fullPools.m_size, retval, retval->m_size);

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s :  %p  => %p ",
                                    L"[ fullPool Change ]", emptyStack, retval);

    return retval;
}

template <>
ObjectPoolType<CMessage> * stTlsObjectPoolManager<CMessage>::GetEmptyPool(ObjectPoolType<CMessage> *fullStack)
{
    ObjectPoolType<CMessage> *retval;

    fullPools.Push(fullStack);
    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s fullPools.m_size :%lld \t InputData  : %p  Data Size : %lld",
                                    L"[ fullPools.Push ]", fullPools.m_size, fullStack, fullStack->m_size);

    if (emptyPools.Pop(retval) == false)
    {
        retval = new ObjectPoolType<CMessage>();
        CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - emptyPools.m_size == 0 ",
                                        L"[ Create New EmptyPool ]", retval);
        return retval;
    }

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s  Size :%lld \t OutData  : %p  Data Size : %lld",
                                    L"[ emptyPools Pop ]", emptyPools.m_size, retval, retval->m_size);

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s  :  %p  => %p ",
                                    L"[ emptyPool Change ]", fullStack, retval);

    return retval;
}



// 직렬화버퍼의 특수화
template <>
PVOID stTlsObjectPool<CMessage>::Alloc()
{
    static ull AllocCnt = 0;
    ull localAllocCnt;

    localAllocCnt = InterlockedIncrement(&AllocCnt);
    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s %08llu ",
                                   L"CMessage Alloc : ", localAllocCnt);
    stTlsObjectPool *pool = nullptr;
    ObjectPoolType<CMessage> *swap;
    if (s_tlsIdx == TLS_OUT_OF_INDEXES)
    {
        CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s ",
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
            CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s %15s %05d",
                                           L"stTlsObjectPool", L"new Return Nullptr ", L"GetLastError : ", GetLastError());
            return nullptr;
        }
        TlsSetValue(s_tlsIdx, pool);
    }

    if (pool->allocPool->m_size == 0)
    {
        swap = pool->allocPool;
        pool->allocPool = instance.GetFullPool(swap);
        return pool->allocPool->Alloc();
        CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%10s PushData : %08p return Data %08p",
                                       L"EmptyPool Push", swap, pool);
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

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%10s %10s : %08p %10s %08p %10s %llu",
                                   L"Alloc ", L"Node ", node, L"PoolAddress ", pool->releasePool, L"m_size", pool->allocPool->m_size);
    return node;
}

template <>
void stTlsObjectPool<CMessage>::Release(PVOID node)
{
    static ull Release = 0;
    ull localReleaseCnt;
    localReleaseCnt = InterlockedIncrement(&Release);

    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%15s %08llu ",
                                   L"CMessage Release : ", localReleaseCnt);

    stTlsObjectPool *pool = nullptr;
    ObjectPoolType<CMessage> *swap;
    if (s_tlsIdx == TLS_OUT_OF_INDEXES)
    {
        CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s ",
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
            CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::ERROR_Mode, L"%10s %15s %15s %05d",
                                           L"stTlsObjectPool", L"new Return Nullptr ", L"GetLastError : ", GetLastError());
            // TODO : 만일 실패하면 Manager 객체에서 바로 가져오는 방안 생각하기.
            return;
        }
        TlsSetValue(s_tlsIdx, pool);
    }

    pool->releasePool->Release(node);
    CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%10s %10s : %08p %10s %08p %10s %llu",
                                   L"Release ", L"Node ", node, L"PoolAddress ", pool->releasePool, L"m_size", pool->releasePool->m_size);

    swap = pool->releasePool;
    if (pool->releasePool->m_size == tlsPool_init_Capacity / 20)
    {

        pool->releasePool = instance.GetEmptyPool(swap);
        CSystemLog::GetInstance()->Log(L"CMessage", en_LOG_LEVEL::DEBUG_Mode, L"%10s PushData : %08p return Data %08p",
                                       L"FullPool Push", swap, pool);
        return;
    }
    //10_31 제거
    // 
    //pool->releasePool = pool->allocPool;
    //pool->allocPool = swap;
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
