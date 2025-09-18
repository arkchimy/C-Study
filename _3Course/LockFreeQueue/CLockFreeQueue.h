#pragma once
#include "../lib/CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"
template <typename T>
class CLockFreeQueue
{
  public:
    CLockFreeQueue();
    ~CLockFreeQueue();
    struct stNode
    {
        T data;
        stNode *next;
    };
    void Push(__in T data);
    T Pop();

    stNode *_head;
    stNode *_tail;

    volatile LONG64 seqNumber;

    CObjectPool<T> pool;
};

template <typename T>
CLockFreeQueue<T>::CLockFreeQueue()
{
    stNode *Dummy = reinterpret_cast<stNode *>(pool.Alloc());
    Dummy->next = nullptr;

    _head = Dummy;
    _tail = _head;
}

template <typename T>
inline CLockFreeQueue<T>::~CLockFreeQueue()
{
    void *ptr = reinterpret_cast<void*>(LONG64(_head) & ADDR_MASK);
    pool.Release(ptr);
}

template <typename T>
void CLockFreeQueue<T>::Push(T data)
{
    LONG64 seq;
    stNode *newNode;

    stNode *tail;
    stNode *tailAddr;

    stNode *tailNext;

    newNode = reinterpret_cast<stNode *>(pool.Alloc());
    newNode->next = nullptr;
    newNode->data = data;

    seq = _interlockedincrement64(&seqNumber);
    newNode = reinterpret_cast<stNode *>(seq << 47 | (LONG64)newNode);
    
    do
    {
        tail = _tail;
        tailAddr = reinterpret_cast<stNode *>((LONG64)tail & ADDR_MASK);
        tailNext = tailAddr->next;

        if (tailNext != nullptr)
        {
            InterlockedCompareExchangePointer((PVOID *)&_tail, tailNext, tail);
            continue;
        }
        if (InterlockedCompareExchangePointer((PVOID *)&(tailAddr->next), newNode, nullptr) == nullptr)
            break;

    } while (1);

    if (InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, tail) != tail)
    {
        InterlockedCompareExchangePointer((PVOID *)&_tail, newNode, tail);
    }
       
}

template <typename T>
T CLockFreeQueue<T>::Pop()
{
 
    stNode *nextNode;
    stNode *headAddr;
    stNode *head;

    stNode *tail;

    stNode *tailNextNode;

    T outData;


    do
    {
        head = _head;
        tail = _tail;

        if (head == tail)
        {
            tailNextNode = reinterpret_cast<stNode *>((ull)tail & ADDR_MASK)->next;
            if (tailNextNode == nullptr)
                continue;
            InterlockedCompareExchangePointer((PVOID *)&_tail, tailNextNode, tail);
        }

        headAddr = reinterpret_cast<stNode *>((LONG64)head & ADDR_MASK);
        nextNode = headAddr->next;
        if (nextNode == nullptr)
            continue;
        outData = reinterpret_cast<stNode *>((LONG64)nextNode & ADDR_MASK)->data;
        if(InterlockedCompareExchangePointer((PVOID *)&_head, nextNode, head) == head)
            break;

    }while(1);

    pool.Release(headAddr);

    return outData;
}
