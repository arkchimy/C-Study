#pragma once
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <vector>

#define SEQ_MASK 0XFFFF800000000000
#define ADDR_MASK 0x00007FFFFFFFFFFF

#define ALLOC_Node 0xcccccccc
#define RELEASE_Node 0xeeeeeeee

using ull = unsigned long long;
using ll = long long;

struct stPoolInfo
{
    stPoolInfo() {}

    int mode1 = 0;   // 보기 편하도록
    int padding = 0; // 보기 편하도록
    ull tag = 0;
    void *addr = nullptr;
    void *nextNode = nullptr; // 내가 누굴 가리키는지

    DWORD threadId = 0;
    int mode2 = 0;
};

template <typename T>
struct stNode
{
    T data;
    stNode *next;
};

extern std::vector<stPoolInfo> pool_infos;

template <typename T>
class CObjectPool
{
  public:
    CObjectPool()
    {
        m_Top = &m_Dummy; // Dummy m_Dummy
        m_Dummy.next = &m_Dummy;
    }
    ~CObjectPool()
    {
        // TODO : 반환되지않은 메모리가 할당 해제되지 못함.
        stNode<T> *StartNode = reinterpret_cast<stNode<T> *>(m_Top);
        LONG64 top_idx = m_size;
        stNode<T> *pCurrentNode = StartNode;

        while (pCurrentNode != &m_Dummy)
        {
            stNode<T> *temp = reinterpret_cast<stNode<T> *>((ll)pCurrentNode & ADDR_MASK);

            pCurrentNode = reinterpret_cast<stNode<T> *>((ll)pCurrentNode & ADDR_MASK)->next;
            // RT_ASSERT(pCurrentNode != temp);
            delete temp;
            _InterlockedDecrement64(&m_size);
        }
        if (m_size != 0)
            __debugbreak();
    }
    void Initalize(DWORD iSize)
    {
        for (DWORD i = 0; i < iSize; i++)
            Release(new stNode<T>());
    }
    PVOID Alloc()
    {
        stNode<T> *oldTop;
        stNode<T> *newTop;
        stNode<T> *ret_Node;

        // Stack에서 빼기
        do
        {
            oldTop = reinterpret_cast<stNode<T> *>(m_Top);

            // 비어있다는 것을 알아야함.
            if (oldTop == &m_Dummy)
            {
                // 정보 세팅
                oldTop = new stNode<T>();
#ifdef POOLTEST

#endif
                return oldTop;
            }
            ret_Node = reinterpret_cast<stNode<T> *>((ll)oldTop & ADDR_MASK);
            newTop = ret_Node->next;

        } while (_InterlockedCompareExchangePointer(&m_Top, newTop, oldTop) != oldTop);

#ifdef POOLTEST

#endif
        _InterlockedDecrement64(&m_size);
        return ret_Node;
    }
    void Release(PVOID newNode)
    {
        // TODO : Push 하기
        stNode<T> *oldTop;
        stNode<T> *newTop;

        ull id = _InterlockedIncrement64((ll *)&seqNumber);
        DWORD iThreadID = GetCurrentThreadId();

        do
        {
            oldTop = reinterpret_cast<stNode<T> *>(m_Top);
            newTop = reinterpret_cast<stNode<T> *>((ll)newNode | id << 47);
            reinterpret_cast<stNode<T> *>(newNode)->next = oldTop;

        } while (_InterlockedCompareExchangePointer(&m_Top, newTop, oldTop) != oldTop);

#ifdef POOLTEST

#endif
        _InterlockedIncrement64(&m_size);
    }

    PVOID m_Top;
    stNode<T> m_Dummy;

    ll seqNumber = -1;
    ll m_size = 0;
};