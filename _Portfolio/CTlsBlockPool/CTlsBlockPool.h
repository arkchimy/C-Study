#pragma once

#include "CBlockPool_UnSafeMT.h"
#include "../../_3Course/lib/CLockFreeStack_lib/CLockFreeStack.h"
using CPoolType = CBlockPool_UnSafeMT;

//Á¤Àû tls·Î »ç¿ëÇÏ´ÂÆíÀÌ ±ò²û.

// Manager °´Ã¼
class CTlsBlockPoolManager
{
    using ManagerPool = CLockFreeStack<CPoolType *>;

  public:
    CTlsBlockPoolManager(size_t tlsPool_init_Capacity, size_t BlockSize)
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

            }
            else
            {
                retval = new CPoolType();
                retval->Initalize(m_tlsPool_init_Capacity, m_BlockSize);


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

            return retval;
        }

        return retval;
    }
    size_t m_tlsPool_init_Capacity = 0;
    size_t m_BlockSize;

    ManagerPool fullPools;
    ManagerPool emptyPools;

    LONG64 m_TotalCount = 0;
};
class CTlsBlockPool
{
  public:

    void InitTlsPool(CTlsBlockPoolManager* pManagersize_t);
    template<typename T>
    T *Alloc() 
    {
        if (m_AllocPool->m_Size == 0)
        {
            m_ManagerInstance->GetFullPool(m_AllocPool);
            return m_AllocPool->Alloc<T>();
        }
        return m_AllocPool->Alloc<T>();

    }

    template <typename T>
    void Release(void* ptr)
    {
        T *inputNode;
        inputNode = reinterpret_cast<T *>(ptr);

        if (m_ReleasePool->m_Size == m_Capacity)
        {
            m_ManagerInstance->GetEmptyPool(m_ReleasePool);
            return m_ReleasePool->Release<T>(inputNode);
        }
        m_ReleasePool->Release<T>(inputNode);
    }

    CTlsBlockPoolManager *m_ManagerInstance = nullptr;
    size_t m_Capacity = 0;
    CPoolType *m_AllocPool = nullptr;
    CPoolType *m_ReleasePool = nullptr;
};
