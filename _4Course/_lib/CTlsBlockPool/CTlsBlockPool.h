#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#include "CLockFreeStack.h"
#include "CBlockPool_UnSafeMT.h"
#include "../../../_3Course/lib/CSystemLog_lib/CSystemLog_lib.h"

using CPoolType = CBlockPool_UnSafeMT;

// 정적 tls로 사용하는편이 깔끔.

// Manager 객체
class CTlsBlockPoolManager
{
    using ManagerPool = CLockFreeStack<CPoolType *>;

  public:
    CTlsBlockPoolManager(int tlsPool_init_Capacity, int BlockSize)
    {
        m_tlsPool_init_Capacity = 300;
        m_BlockSize = BlockSize;
    }
    CPoolType *GetFullPool(CPoolType *emptyStack)
    {
        CPoolType *retval;

        emptyPools.Push(emptyStack);

        if (fullPools.Pop(retval) == false)
        {
            if (emptyPools.Pop(retval))
            {
                retval->Initalize(m_tlsPool_init_Capacity, m_BlockSize);
                CSystemLog::GetInstance()->Log(L"tlsBlockPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                               L"[ new tlsPool_init_Capacity => FullPool ]", retval);
            }
            else
            {
                retval = new CPoolType();
                retval->Initalize(m_tlsPool_init_Capacity, m_BlockSize);
                CSystemLog::GetInstance()->Log(L"tlsBlockPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - fullPools.m_size == 0 ",
                                               L"[ Create New FullPool ]", retval);
            }
            return retval;
        }

        return retval;
    }
    CPoolType *GetEmptyPool(CPoolType *fullStack)
    {
        CPoolType *retval;

        fullPools.Push(fullStack);

        if (emptyPools.Pop(retval) == false)
        {
            retval = new CPoolType();
            CSystemLog::GetInstance()->Log(L"tlsBlockPool", en_LOG_LEVEL::SYSTEM_Mode, L"%15s : %p - emptyPools.m_size == 0 ",
                                           L"[ Create New EmptyPool ]", retval);
            return retval;
        }

        return retval;
    }
    int m_tlsPool_init_Capacity = 0;
    int m_BlockSize;

    ManagerPool fullPools;
    ManagerPool emptyPools;

    LONG64 m_TotalCount = 0;
};
class CTlsBlockPool
{
  public:
    inline void InitTlsPool(CTlsBlockPoolManager* pManagersize_t)
    {
          m_ManagerInstance = pManagersize_t;
          m_AllocPool = new CPoolType();
          m_AllocPool->Initalize(m_ManagerInstance->m_tlsPool_init_Capacity, m_ManagerInstance->m_BlockSize);
          m_ReleasePool = new CPoolType();
          m_ReleasePool->Initalize(m_ManagerInstance->m_tlsPool_init_Capacity, 0);
    }
    template <typename T>
    T *Alloc()
    {
        if (m_AllocPool->m_Size == 0)
        {
            m_AllocPool = m_ManagerInstance->GetFullPool(m_AllocPool);
            return m_AllocPool->Alloc<T>();
        }
        return m_AllocPool->Alloc<T>();
    }

    template <typename T>
    void Release(void *ptr)
    {
        T *inputNode;
        inputNode = reinterpret_cast<T *>(ptr);

        if (m_ReleasePool->m_Size == m_Capacity)
        {
            m_ReleasePool = m_ManagerInstance->GetEmptyPool(m_ReleasePool);
            return m_ReleasePool->Release<T>(inputNode);
        }
        m_ReleasePool->Release<T>(inputNode);
    }

    CTlsBlockPoolManager *m_ManagerInstance = nullptr;
    size_t m_Capacity = 0;
    CPoolType *m_AllocPool = nullptr;
    CPoolType *m_ReleasePool = nullptr;
};
