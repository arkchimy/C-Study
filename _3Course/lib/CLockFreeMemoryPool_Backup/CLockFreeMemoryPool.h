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

    int mode1 = 0; // 보기 편하도록
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
    stNode()
        : stNode(0)
    {
    }

    stNode(DWORD threadID)
        : ownerThreadID(threadID)
    {
        next = nullptr;
    }
    T data;
    stNode *next;
    DWORD ownerThreadID;
};

extern std::vector<stPoolInfo> pool_infos;

template <typename T>
class CObjectPool
{
  public:
    CObjectPool()
    {
        m_Top = &m_Dummy; //Dummy m_Dummy
        m_Dummy.next = &m_Dummy;
    }
    ~CObjectPool()
    {
        // TODO : 반환되지않은 메모리가 할당 해제되지 못함.
        stNode<T> *StartNode = reinterpret_cast<stNode<T>*>(m_Top);
        long top_idx = iNodeCnt;
        stNode<T> *pCurrentNode = StartNode ;

        while (pCurrentNode != &m_Dummy)
        {
            stNode<T> *temp = reinterpret_cast<stNode<T> *>((ll)pCurrentNode & ADDR_MASK);

            pCurrentNode = reinterpret_cast<stNode<T> *>((ll)pCurrentNode & ADDR_MASK)->next;
            //RT_ASSERT(pCurrentNode != temp);
            delete temp;
            _InterlockedDecrement64(&iNodeCnt);
        }
        if (iNodeCnt != 0)
            __debugbreak();
    }
    PVOID Alloc()
    {
        stNode<T> *oldTop;
        stNode<T> *newTop;
        stNode<T> *ret_Node;

        DWORD iThreadID;
        ll currentSeqNumber;

        iThreadID = GetCurrentThreadId();

        // Stack에서 빼기
        do
        {
            oldTop = reinterpret_cast<stNode<T>*>(m_Top);
            
            // 비어있다는 것을 알아야함.
            if (oldTop == &m_Dummy)
            {
                // 정보 세팅
                oldTop = new stNode<T>(iThreadID);
                _InterlockedIncrement64(&iNodeCnt);
#ifdef POOLTEST
   
#endif
                return oldTop;
            }
            ret_Node = reinterpret_cast<stNode<T> *>((ll)oldTop & ADDR_MASK);
            newTop = ret_Node->next;


        } while (_InterlockedCompareExchangePointer(&m_Top, newTop, oldTop) != oldTop);

#ifdef POOLTEST
 
#endif
        if (ret_Node->ownerThreadID != 0)
            __debugbreak();
        ret_Node->ownerThreadID = iThreadID;

        return ret_Node;
    }
    void Release(PVOID newNode)
    {
        // TODO : Push 하기
        stNode<T> *oldTop;
        stNode<T> *newTop;

        ull id = _InterlockedIncrement64((ll *)&seqNumber);
        DWORD iThreadID = GetCurrentThreadId();

        if (reinterpret_cast<stNode<T> *>(newNode)->ownerThreadID == 0)
            __debugbreak();
        do
        {
            oldTop = reinterpret_cast<stNode<T>*>(m_Top);
            newTop = reinterpret_cast<stNode<T>*>((ll)newNode | id << 47);
            reinterpret_cast<stNode<T>*>(newNode)->next = oldTop;
            reinterpret_cast<stNode<T> *>(newNode)->ownerThreadID = 0;
        } while (_InterlockedCompareExchangePointer(&m_Top, newTop, oldTop) != oldTop);

#ifdef POOLTEST

#endif
    }

    PVOID m_Top;
    stNode<T> m_Dummy;

    ll seqNumber = -1;
    ll iNodeCnt = 0;
};